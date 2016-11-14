#include <vector>
#include <list>

#include <nan.h>
#include <v8.h>
#include <node.h>

#include <FLAC++/decoder.h>
#include <FLAC++/encoder.h>

namespace nodeflac {
	using namespace ::v8;
	using namespace ::node;
	using namespace ::FLAC;

	template <typename T>
	static const T GetOr(Local<Object>& src, const char* key, const T def) /* !! uses current Isolate !! */ {
		MaybeLocal<Value> value = Nan::Get(src, Nan::New(key).ToLocalChecked());
		if (value.IsEmpty()) {
			return def;
		} else {
			return Nan::To<T>(value.ToLocalChecked()).FromJust();
		}
	}

	class FlacEncodeStream : public Nan::ObjectWrap, public Encoder::Stream {
	public:
		typedef ::FLAC__StreamEncoderWriteStatus Status;
		typedef std::list<MaybeLocal<Object>>         BufferList;

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
			
			init();
		}

		static void Init   (Local<Object> exports);

		static void New    (const FunctionCallbackInfo<Value>& args);
		static void Process(const FunctionCallbackInfo<Value>& args);
	protected:
		Status write_callback (const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame) {
			buffers.push_back(Nan::CopyBuffer((const char*) buffer, bytes));
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

	void FlacEncodeStream::Process(const FunctionCallbackInfo<Value>& args) {
		/* process a buffer and return a buffer (sync) */
		Nan::HandleScope scope;
		Isolate* isolate = args.GetIsolate();

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
		self->process(pointers, smallestLen / self->sampleSize);

		/* collect results */
		Local<Array> rv = Array::New(isolate, self->buffers.size());

		int index = 0;
		for (MaybeLocal<Object>& buf : self->buffers) {
			rv->Set(index++, buf.ToLocalChecked());
		}

		self->buffers.clear();
		args.GetReturnValue().Set(rv);
	}

	static void Initialize(Handle<Object> target) {
        Nan::HandleScope scope;
        
        FlacEncodeStream::Init(target);
    }
}


NODE_MODULE(bindings, nodeflac::Initialize);