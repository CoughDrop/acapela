#include <node.h>
#include <math.h>
#include <Windows.h>
#include "include\ioBabTTS.h" 
#define _BABTTSDYN_IMPL_
#include "include\ifBabTTSDyn.h" 
#undef _BABTTSDYN_IMPL_
#include "include\license.h" 

// node-gyp configure
// node-gyp build
// npm install ../acapela

namespace acapela {

	using v8::Local;
	using v8::Context;
	using v8::Persistent;
	using v8::Isolate;
	using v8::FunctionCallbackInfo;
	using v8::Object;
	using v8::HandleScope;
	using v8::String;
	using v8::NewStringType;
	using v8::Boolean;
	using v8::Array;
	using v8::Number;
	using v8::Value;
	using v8::Null;
	using v8::Function;
	using node::AtExit;


	static LPBABTTS babtts = 0;

	static bool already_setup = false;
	static double downloadPercent = 0;
	static char * last_voice = 0;
	static bool dll_ready = false;
	static bool initialized = false;

	bool setup() {
		printf("Setting Up...\n");
		if (already_setup) {
			printf("Already Set Up\n");
			return true;
		}
		bool success;
		if (!dll_ready) {
			printf("Initializing DLL\n");
			success = BabTtsInitDll();
			dll_ready = true;
		}

		printf("Setting License\n");
		CTTSBundle bundle(CTTSBundle::BUNDLE_BABTTS);
		bundle.SetLicense();

		if (!initialized) {
			printf("Initializing\n");
			success &= BabTTS_Init();
			initialized = true;
		}

		if(success) {
  			already_setup = true;
  		}

		return success;
	}

	void jsStatus(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();
		bool ready = setup();
		Local<Object> obj = Object::New(isolate);
		obj->Set(context, String::NewFromUtf8(isolate, "ready", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, ready));
		args.GetReturnValue().Set(obj);
	}

	bool closeVoice() {
		printf("BabTTS_Close %s\n", last_voice);
		if (babtts != 0) {
			printf("Closing babtts\n");
			BabTTS_Close(babtts);
			babtts = 0;
		}
		if (last_voice != 0) {
			printf("Freeing last_voice %s\n", last_voice);
			free(last_voice);
			last_voice = 0;
		}
		return true;
	}

	bool teardown() {
		if (!already_setup) { return true; }
		bool success;
    
		printf("Tearing down \n");
		closeVoice();
		if (initialized) {
			printf("Uninitializing \n");
			success = BabTTS_Uninit();
			initialized = false;
		}
		if (dll_ready) {
			printf("Uninitiailizing DLL\n");
			BabTtsUninitDll();
			dll_ready = false;
		}

		babtts = 0;
		last_voice = 0;
		already_setup = false;
		return success;
	}

	Local<Array> listVoices(Isolate* isolate) {
		teardown();
		setup();
		Local<Context> context = isolate->GetCurrentContext();
		BabTtsError babError = E_BABTTS_NOERROR;
		BABTTSINFO voiceInfo; 
		long tempnumber = BabTTS_GetNumVoices(); 
		Local<Array> result = Array::New(isolate, tempnumber);
		printf("Number of voice: %d\n", tempnumber); 
		for (int j = 0; j<tempnumber; j++) { 
			char szVoice[50]; 
			szVoice[0] = '\0'; 
			memset(&voiceInfo, 0, sizeof(BABTTSINFO)); 
			babError = BabTTS_EnumVoices(j, szVoice); 
			if (babError != E_BABTTS_NOERROR) { 
				printf("BabTTS_EnumVoices error: %d\n", babError); 
			}
			else {
				babError = BabTTS_GetVoiceInfo(szVoice, &voiceInfo);
				if (babError != E_BABTTS_NOERROR) {
					printf("BabTTS_GetVoiceInfo error: %d\n", babError);
				}
				else {
					Local<Object> obj = Object::New(isolate);
					obj->Set(context, String::NewFromUtf8(isolate, "voice_id", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, voiceInfo.szName, NewStringType::kNormal).ToLocalChecked());
					obj->Set(context, String::NewFromUtf8(isolate, "name", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, voiceInfo.szSpeaker, NewStringType::kNormal).ToLocalChecked());
					obj->Set(context, String::NewFromUtf8(isolate, "locale", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, "en-US", NewStringType::kNormal).ToLocalChecked());
					obj->Set(context, String::NewFromUtf8(isolate, "language", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, voiceInfo.szLanguage, NewStringType::kNormal).ToLocalChecked());
					obj->Set(context, String::NewFromUtf8(isolate, "active", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, true));

					result->Set(context, j, obj); // String::Utf8Value("asdf"));
					printf("Voice: %s Speaker: %s Language: %s Version: %s\n", voiceInfo.szName, voiceInfo.szSpeaker, voiceInfo.szLanguage, voiceInfo.szVersion);
				}
			}
		}
		return result;
	}

	bool openVoice(const char * voice_string) {
		if (!already_setup) { setup(); }
		BabTtsError error;

		if (last_voice != 0 && strcmp(last_voice, voice_string) == 0) {
			return true;
		} else if(last_voice != 0) {
			printf("Closing last voice before opening new one %s %s\n", last_voice, voice_string);
		  closeVoice();
		}

		printf("BabTTS_Create %s, %s\n", last_voice, voice_string);
		babtts = BabTTS_Create(); 
		printf("BabTTS_Open with %s\n", voice_string);
		error = BabTTS_Open(babtts, voice_string, BABTTS_USEDEFDICT);
		if (error != E_BABTTS_NOERROR) { 
			printf("Error while opening voice: %d\n", error); 
			return false;
		}
		else { 
		  last_voice = (char *) malloc(strlen(voice_string) + 1);
		  strcpy(last_voice, voice_string);
			return true;
		}
	}

	static bool isSpeaking;

	BabTtsError WINAPI BabTTSProc(LPBABTTS lpBabTTS, DWORD Msg, DWORD_PTR dwUserData, DWORD_PTR dwMsgInfo) {
		printf("Callback triggered\n");
		switch (Msg)
		{
		case BABTTS_MSG_END:
			printf("Done Speaking\n");
			isSpeaking = false;
			break;
		}
		return E_BABTTS_NOERROR;
	}

	bool speakText(Isolate * isolate, Local<Object> opts) {
		Local<Context> context = isolate->GetCurrentContext();
		isSpeaking = true;
		BabTtsError error;
		// String::Utf8Value string8(Local<String>::Cast(opts->Get(context, String::NewFromUtf8(isolate, "text", NewStringType::kNormal).ToLocalChecked())));
		String::Utf8Value string8(opts->Get(context, String::NewFromUtf8(isolate, "text", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()->ToString());
		const char * lpszMyText = *string8;
		double speed = opts->Get(context, String::NewFromUtf8(isolate, "rate", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()->NumberValue();
		double volume = opts->Get(context, String::NewFromUtf8(isolate, "volume", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()->NumberValue();
		double pitch = opts->Get(context, String::NewFromUtf8(isolate, "pitch", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked()->NumberValue();
		printf("values: %G %G %G\n", speed, volume, pitch);
		if (!speed || speed == 0 || isnan(speed)) {
			speed = 100;
		}
		if (!volume || volume == 0 || isnan(volume)) {
			volume = 100;
		}
		if (!pitch || pitch == 0 || isnan(pitch)) {
			pitch = 100;
		}

		error = BabTTS_SetSettings(babtts, BABTTS_PARAM_SPEED, (DWORD_PTR) speed);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while setting speech rate: %d\n", error);
			return false;
		}

		error = BabTTS_SetSettings(babtts, BABTTS_PARAM_VOCALTRACT, (DWORD_PTR) pitch);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while setting speech pitch: %d\n", error);
			return false;
		}

		error = BabTTS_SetSettings(babtts, BABTTS_PARAM_VOLUMERATIO, (DWORD_PTR) volume);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while setting speech volume: %d\n", error);
			return false;
		}
		Local<Function> func = Local<Function>::Cast(opts->Get(context, String::NewFromUtf8(isolate, "success", NewStringType::kNormal).ToLocalChecked()).ToLocalChecked());
		int mode = BABTTS_ASYNC;
		// if (sync) { mode = BABTTS_SYNC; }
		error = BabTTS_SetCallback(babtts, BabTTSProc, BABTTS_CB_FUNCTION);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while setting callback: %d\n", error);
			return false;
		}
		error = BabTTS_Speak(babtts, lpszMyText, BABTTS_TEXT | mode | BABTTS_TXT_ANSI);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while speaking: %d\n", error);
			return false;
		}
		return true;
	}

	bool stopSpeakingText() {
		BabTtsError error = BabTTS_Reset(babtts);
		if (error != E_BABTTS_NOERROR) {
			printf("Error while stopping speaking: %d\n", error);
			return false;
		}
		return true;
	}

	void jsSetup(const FunctionCallbackInfo<Value>& args) {
		bool result = setup();
		args.GetReturnValue().Set(result);
	}

	void jsTeardown(const FunctionCallbackInfo<Value>& args) {
		bool result = teardown();
		args.GetReturnValue().Set(result);
	}

	void jsSpeak(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		Local<Object> opts = Local<Object>::Cast(args[0]);
		bool result = speakText(isolate, opts);
		args.GetReturnValue().Set(result);
	}

	void jsStopSpeaking(const FunctionCallbackInfo<Value>& args) {
		bool result = stopSpeakingText();
		args.GetReturnValue().Set(result);
	}

	void jsSpeakCheck(const FunctionCallbackInfo<Value>& args) {
		args.GetReturnValue().Set(isSpeaking);
	}

	void jsListVoices(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		Local<Array> result = listVoices(isolate);
		args.GetReturnValue().Set(result);
	}

	void jsOpenVoice(const FunctionCallbackInfo<Value>& args) {
		String::Utf8Value string(args[0]);
		const char * voice_string = *string;
		bool result = openVoice(voice_string);
		args.GetReturnValue().Set(result);
	}

	void jsCloseVoice(const FunctionCallbackInfo<Value>& args) {
		bool result = closeVoice();
		args.GetReturnValue().Set(result);
	}

	void init(Local<Object> exports) {
		NODE_SET_METHOD(exports, "status", jsStatus);
		NODE_SET_METHOD(exports, "init", jsSetup);
		NODE_SET_METHOD(exports, "teardown", jsTeardown);
		NODE_SET_METHOD(exports, "speakText", jsSpeak);
		NODE_SET_METHOD(exports, "stopSpeakingText", jsStopSpeaking);
		NODE_SET_METHOD(exports, "isSpeaking", jsSpeakCheck);
		NODE_SET_METHOD(exports, "getAvailableVoices", jsListVoices);
		NODE_SET_METHOD(exports, "openVoice", jsOpenVoice);
		NODE_SET_METHOD(exports, "closeVoice", jsCloseVoice);
	}

	NODE_MODULE(acapela, init)
}