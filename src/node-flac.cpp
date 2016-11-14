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

	class FlacEncodeStreamInternal : public Encoder::Stream {
	public:
		typedef ::FLAC__StreamEncoderWriteStatus Status;
		typedef std::vector<uint8_t>             Buffer;
		typedef std::list<Buffer>                BufferList;
		
		FlacEncodeStreamInternal(bool streaming, unsigned int numChannels, unsigned int depth, unsigned int rate) {
			set_streamable_subset (streaming);
			set_sample_rate       (rate);
			set_bits_per_sample   (depth);
			set_channels          (numChannels);
			
			init();
		}

		BufferList& Buffers() {
			return buffers;
		}

	protected:
		Status write_callback (const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame) {

			buffers.push_back(
				std::vector<uint8_t>(
					(const uint8_t*) buffer, 
					(const uint8_t*) buffer + bytes));

			return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
		}
	private:
		BufferList buffers;
	};

	class FlacEncodeStream : public Nan::ObjectWrap {
	private:
		unsigned int numChannels, sampleSize;
		FlacEncodeStreamInternal  flac;
		static Persistent<Object> constructor;
	public:
		FlacEncodeStream(bool streaming, unsigned int _numChannels, unsigned int _depth, unsigned int rate)
			: numChannels(_numChannels), sampleSize(_depth / 8),
			  flac(streaming, _numChannels, _depth, rate)
		{}

		static void Init   (Local<Object> exports);

		static void New    (const FunctionCallbackInfo<Value>& args);
		static void Process(const FunctionCallbackInfo<Value>& args);
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
		
		/* non-interleaved, i.e. multi channel */
		self->flac.process(pointers, smallestLen / self->sampleSize);

		/* submit buffers for processing */
		FlacEncodeStreamInternal::BufferList& buffers = self->flac.Buffers();
		Local<Array> rv = Array::New(isolate, buffers.size());

		int index = 0;
		for (FlacEncodeStreamInternal::Buffer& buf : buffers) {
			rv->Set(index++, Nan::CopyBuffer((const char *) &buf.front(), buf.size()).ToLocalChecked());
		}

		args.GetReturnValue().Set(rv);
	}

	static void Initialize(Handle<Object> target) {
        Nan::HandleScope scope;

        FlacEncodeStream::Init(target);
    }
}


NODE_MODULE(bindings, nodeflac::Initialize);