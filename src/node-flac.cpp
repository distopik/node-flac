#include <vector>
#include <list>

#include <nan.h>
#include <v8.h>
#include <node.h>

#include <atomic>
#include <cstddef>

#include <FLAC++/decoder.h>
#include <FLAC++/encoder.h>

namespace nodeflac {
	using namespace ::v8;
	using namespace ::node;
	using namespace ::FLAC;

	template<typename ET, int S, typename ST=int>
	class ringbuffer
	{
	public:
		typedef ET value_type;
		typedef ST size_type;
	    
		ringbuffer()
		{
			clear();
		}
	    
		~ringbuffer() {}
	    
		size_type size()     const { return m_size; }
		size_type max_size() const { return S; }
	    
		bool empty() const	{ return m_size == 0; }
		bool full()  const	{ return m_size == S; }
	    
		value_type& front() { return m_buffer[m_front]; }
		value_type& back() { return m_buffer[m_back]; }
	    
		void clear() {
			m_size = 0;
			m_front = 0;
			m_back  = S - 1;
		}
	    
		void push()	{
			m_back = (m_back + 1) % S;
			if( size() == S )
				m_front = (m_front + 1) % S;
			else
				m_size++;
		}
	    
		void push(const value_type& x) {
			push();
			back() = x;
		}
	    
		void pop() {
			if( m_size > 0  ) {
				m_size--;
				m_front = (m_front + 1) % S;
			}
		}
	    
		void back_erase(const size_type n) {
			if( n >= m_size )
				clear();
			else {
				m_size -= n;
				m_back = (m_front + m_size - 1) % S;
			}
		}
	    
		void front_erase(const size_type n) {
			if( n >= m_size )
				clear();
			else {
				m_size -= n;
				m_front = (m_front + n) % S;
			}
		}
	    
	protected:
	    
		value_type m_buffer[S];
	    
		size_type m_size;
	    
		size_type m_front;
		size_type m_back;
	};



	template <typename T>
	static const T GetOr(Local<Object>& src, const char* key, const T def) /* !! uses current Isolate !! */ {
		Local<Value> lookup = Nan::New(key).ToLocalChecked();

		if (!src->Has(lookup)) {
			return def;
		} else {
			MaybeLocal<Value> value = Nan::Get(src, lookup);
			if (value.IsEmpty()) {
				return def;
			}

			return Nan::To<T>(value.ToLocalChecked()).FromJust();
		}
	}

	class FlacEncodeStream : public Nan::ObjectWrap, public Encoder::Stream {
	public:
		typedef ::FLAC__StreamEncoderWriteStatus Status;
		typedef ringbuffer<uint8_t, 16 * 1024>   BufferList;

	private:
		unsigned int numChannels, sampleSize;
		BufferList   buffers;

		static Persistent<Object> constructor;

	public:
		FlacEncodeStream(bool _streaming, unsigned int _numChannels, unsigned int _depth, unsigned int _rate)
			: numChannels(_numChannels), sampleSize(_depth / 8)
		{
			set_streamable_subset (_streaming);
			set_sample_rate       (_rate);
			set_bits_per_sample   (_depth);
			set_channels          (_numChannels);
			set_blocksize         (_streaming ? 128 : 0);

			printf("FlacEncodeStream: streaming: %s, numChannels: %u, depth: %u, rate: %u\n",
				_streaming ? "true" : "false", _numChannels, _depth, _rate);
			
			init();
		}

		static void Init   (Local<Object> exports);

		static void New           (const FunctionCallbackInfo<Value>& args);
		static void Process       (const FunctionCallbackInfo<Value>& args);
		static void Finish        (const FunctionCallbackInfo<Value>& args);
			   void ReturnBuffers (const FunctionCallbackInfo<Value>& args);
	protected:
		Status write_callback (const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame) {
			printf(" ++ %zu bytes\n", bytes);
			for (size_t i = 0; i < bytes; i++) {
				buffers.push(buffer[i]);
			}
			return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
		}
	};

	Persistent<Object> FlacEncodeStream::constructor;

	void FlacEncodeStream::Init(Local<Object> exports) {
		Isolate* isolate = exports->GetIsolate();
		Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
		tpl->SetClassName(String::NewFromUtf8(isolate, "FlacEncodeStream"));
		tpl->InstanceTemplate()->SetInternalFieldCount(1);

		// Prototype
		NODE_SET_PROTOTYPE_METHOD(tpl, "process", Process);
		NODE_SET_PROTOTYPE_METHOD(tpl, "finish",  Finish);

		constructor.Reset(isolate, tpl->GetFunction());
		exports->Set(String::NewFromUtf8(isolate, "FlacEncodeStream"), tpl->GetFunction());
	}

	void FlacEncodeStream::New (const FunctionCallbackInfo<Value>& args) {
		if (args.IsConstructCall()) {
			Local<Object> cfg = args[0].As<Object>();

			// Invoked as constructor: `new FlacEncodeStream(...)`
			bool         streaming   = GetOr(cfg, "streaming",   false);
			unsigned int numChannels = GetOr(cfg, "numChannels", 2);
			unsigned int depth       = GetOr(cfg, "depth",       16);
			unsigned int rate        = GetOr(cfg, "rate",        44100);

			FlacEncodeStream* obj = new FlacEncodeStream(streaming, numChannels, depth, rate);
			obj->Wrap(args.This());
			args.GetReturnValue().Set(args.This());
		}
	}

	void FlacEncodeStream::Finish(const FunctionCallbackInfo<Value>& args) {
		Nan::HandleScope scope;

		/* get "this" */
		FlacEncodeStream* self = ObjectWrap::Unwrap<FlacEncodeStream>(args.Holder());

		self->finish();
		self->ReturnBuffers(args);
	}

	void FlacEncodeStream::Process(const FunctionCallbackInfo<Value>& args) {
		/* process a buffer and return a buffer (sync) */
		Nan::HandleScope scope;

		/* get "this" */
		FlacEncodeStream* self = ObjectWrap::Unwrap<FlacEncodeStream>(args.Holder());

		FLAC__int32* pointers[64];
		Local<Array> array = args[0].As<Array>();

		size_t smallestLen = ~0;
		for (unsigned int i = 0; i < self->numChannels; i++) {
			pointers[i] = (FLAC__int32*) Buffer::Data(array->Get(i));
			smallestLen = std::min(smallestLen, Buffer::Length(array->Get(i)));
		}
		
		/* submit buffers for processing */
		printf("... Submit!\n");
		self->process(pointers, smallestLen / self->sampleSize);
		printf("... After submit!\n");

		self->ReturnBuffers(args);
	}

	void FlacEncodeStream::ReturnBuffers(const FunctionCallbackInfo<Value>& args) {
		size_t        sz  = buffers.size();
		Local<Object> buf = Nan::NewBuffer(sz).ToLocalChecked();
		uint8_t*      dta = (uint8_t*) Buffer::Data(buf);

		printf(" -- %zu bytes\n", sz);

		while (!buffers.empty()) {
			*(dta++) = buffers.front();
			buffers.pop();
		}

		// self->buffers.clear();
		args.GetReturnValue().Set(buf);
	}

	static void Initialize(Handle<Object> target) {
        Nan::HandleScope scope;

        FlacEncodeStream::Init(target);
    }
}


NODE_MODULE(bindings, nodeflac::Initialize);