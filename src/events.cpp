#include <algorithm>
#include <vector>
#include "uv.h"
#include "node.h"
#include "plugincommon.h"
#include "amx/amx.h"
#include "events.hpp"
#include "utils.hpp"
#include "logger.hpp"

namespace sampnode
{
	eventsContainer events = eventsContainer();

	bool event::register_event(const std::string& eventName, const std::string& param_types)
	{
		if (events.find(eventName) != events.end()) return false;
		events.insert({ eventName, new event(eventName, param_types) });
		return true;
	}

	void event::register_event(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		if (info.Length() > 1)
		{
			auto isolate = info.GetIsolate();
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope scope(isolate);

			v8::Local<v8::Context> context = isolate->GetCurrentContext();
			v8::Context::Scope contextScope(context);

			if (!info[0]->IsString() || !info[1]->IsString())
			{
				info.GetReturnValue().Set(false);
				return;
			}
			else
			{
				std::string eventName = utils::js_to_string(isolate, info[0]);
				std::string paramTypes = utils::js_to_string(isolate, info[1]);
				if (events.find(eventName) != events.end())
				{
					info.GetReturnValue().Set(false);
					return;
				}
				events.insert({ eventName, new event(eventName, paramTypes) });
				info.GetReturnValue().Set(true);
			}
		}
	}

	void event::on(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		if (info.Length() > 0)
		{
			auto isolate = info.GetIsolate();
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope scope(isolate);

			v8::Local<v8::Context> context = isolate->GetCurrentContext();
			v8::Context::Scope contextScope(context);

			if (!info[0]->IsString())
				return;

			int funcArgIndex = info.Length() - 1;
			std::string eventName = utils::js_to_string(isolate, info[0]);

			if (events.find(eventName) == events.end()) return;
			event* _event = events[eventName];

			if ((funcArgIndex >= 0) && (info[funcArgIndex]->IsFunction()))
			{
				v8::Local<v8::Function> function = info[funcArgIndex].As<v8::Function>();
				_event->append(context, function);
			}
		}
	}

	void event::remove_listener(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		if (info.Length() > 0)
		{
			auto isolate = info.GetIsolate();
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope scope(isolate);

			v8::Local<v8::Context> _context = isolate->GetCurrentContext();

			v8::Context::Scope contextScope(_context);

			if (!info[0]->IsString())
				return;

			std::string eventName = utils::js_to_string(isolate, info[0]);

			if (events.find(eventName) == events.end()) return;

			event* _event = events[eventName];

			if (info.Length() > 1)
			{
				if (info[1]->IsArray())
				{
					v8::Local<v8::Array> funcArray = v8::Local<v8::Array>::Cast(info[1]);
					for (unsigned int i = 0; i < funcArray->Length(); i++)
					{
						const v8::Local<v8::Function>& function = funcArray->Get(_context, i).ToLocalChecked().As<v8::Function>();
						for (auto& element : _event->functionList)
						{
							if (element.context == _context && element.function.Get(isolate) == function)
							{
								_event->remove(element);
								break;
							}
						}

					}
				}
				else
				{
					v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(info[1]);
					for (auto& element : _event->functionList) {
						if (element.context == _context && element.function.Get(isolate) == function)
						{
							_event->remove(element);
							break;
						}
					}
				}
			}
			else if (info.Length() == 1)
			{
				auto copiedFunctionList = _event->functionList;

				for (auto& element : copiedFunctionList) {
					if (element.context == _context)
					{
						_event->remove(element);
					}
				}
			}
		}
	}

	void event::fire(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		if (info.Length() > 0)
		{
			auto isolate = info.GetIsolate();
			v8::Locker locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope scope(isolate);

			if (!info[0]->IsString())
				return;

			std::string eventName = utils::js_to_string(isolate, info[0]);

			if (events.find(eventName) == events.end()) return;
			event* _event = events[eventName];

			v8::Local<v8::Value>* argv = NULL;
			unsigned int argc = info.Length() - 1;
			argv = new v8::Local<v8::Value>[argc];

			for (int i = 0; i < argc; i++)
			{
				argv[i] = info[i + 1];
			}

			_event->call(argv, argc);
		}
	}

	cell event::pawn_call_event(AMX* amx, cell* params)
	{
		char* eventName_c;
		amx_StrParam(amx, params[1], eventName_c);
		const std::string eventName(eventName_c);

		const auto& _event = events.find(eventName);
		if (_event == events.end()) return 0;

		cell retVal = 0;
		_event->second->call_from_pawn_native(amx, params + 1, &retVal);
		return retVal;
	}

	event::event(const std::string& eventName, const std::string& param_types)
		: name(eventName),
		paramTypes(param_types),
		functionList(std::vector<EventListener_t>())
	{
	}

	event::event()
	{

	}

	event::~event()
	{

	}

	void event::append(const v8::Local<v8::Context>& context, const v8::Local<v8::Function>& function)
	{
		v8::Isolate* isolate = function->GetIsolate();

		bool result = std::any_of(functionList.cbegin(), functionList.cend(),
			[&function, &isolate](const EventListener_t& listener)
			{
				return listener.function.Get(isolate) == function;
			});

		if (result)
		{
			return;
		}

		functionList.push_back(
			EventListener_t(
				isolate,
				context,
				function
			)
		);
	}

	void event::remove(const EventListener_t& eventListener)
	{
		functionList.erase(std::remove(functionList.begin(), functionList.end(), eventListener), functionList.end());
	}

	void event::remove_all()
	{
		functionList.clear();
	}

	void event::call(v8::Local<v8::Value>* args, int argCount)
	{
		std::vector<EventListener_t> copiedFunctionList = functionList;

		for (auto& listener : copiedFunctionList)
		{
			if (std::find(functionList.begin(), functionList.end(), listener) == functionList.end()) {
				continue;
			}

			v8::Isolate* isolate = listener.isolate;
			v8::Locker v8Locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope hs(isolate);
			v8::Local<v8::Context> ctx = listener.context.Get(isolate);
			v8::Context::Scope cs(ctx);

			isolate->CancelTerminateExecution();

			v8::TryCatch eh(isolate);

			v8::Local<v8::Function> function = listener.function.Get(isolate);
			function->Call(ctx, ctx->Global(), argCount, args);

			if (argCount > 0) delete[] args;

			if (eh.HasCaught())
			{
				v8::String::Utf8Value str(isolate, eh.Exception());
				v8::String::Utf8Value stack(isolate, eh.StackTrace(listener.context.Get(isolate)).ToLocalChecked());

				L_ERROR << "Event handling function in resource: " << *str << "\nstack:\n" << *stack << "\n";
			}
		}
	}

	void event::call(AMX* amx, cell* params, cell* retval)
	{
		std::vector<EventListener_t> copiedFunctionList = functionList;

		for (auto& listener : copiedFunctionList)
		{
			if (std::find(functionList.begin(), functionList.end(), listener) == functionList.end()) {
				continue;
			}

			v8::Isolate* isolate = listener.isolate;
			v8::Locker v8Locker(isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope hs(isolate);
			v8::Local<v8::Context> ctx = listener.context.Get(isolate);
			v8::Context::Scope cs(ctx);

			isolate->CancelTerminateExecution();

			v8::TryCatch eh(isolate);

			v8::Local<v8::Value>* argv = NULL;
			unsigned int argc = paramTypes.length();
			argv = new v8::Local<v8::Value>[argc];

			for (unsigned int i = 0; i < argc; i++)
			{
				switch (paramTypes[i])
				{
				case 's':
				{
					cell* maddr = NULL;
					int len = 0;
					char* sval;
					if (amx_GetAddr(amx, params[i + 1], &maddr) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get string address: " << name.c_str();
						return;
					}
					amx_StrLen(maddr, &len);
					sval = new char[len + 1];
					if (amx_GetString(sval, maddr, 0, len + 1) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get string address: " << name.c_str();
						return;
					}
					argv[i] = v8::String::NewFromUtf8(isolate, sval).ToLocalChecked();
					break;
				}
				case 'a':
				{
					cell* array = NULL;
					if (amx_GetAddr(amx, params[i + 1], &array) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get array address: " << name.c_str();
						return;
					}
					int size = params[i + 2];
					L_INFO << "Array size: " << size;
					v8::Local<v8::Array> jsArray = v8::Array::New(isolate, size);
					for (int j = 0; j < size; j++)
					{
						jsArray->Set(ctx, j, v8::Integer::New(isolate, static_cast<uint32_t>(array[j])));
					}
					argv[i] = jsArray;
					i++;
					break;
				}
				case 'v':
				{
					cell* array = NULL;
					if (amx_GetAddr(amx, params[i + 1], &array) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get float array address: " << name.c_str();
						return;
					}

					int size = params[i + 2];
					v8::Local<v8::Array> jsArray = v8::Array::New(isolate, size);
					for (int j = 0; j < size; j++)
					{
						jsArray->Set(ctx, j, v8::Integer::New(isolate, amx_ctof(array[j])));
					}
					argv[i] = jsArray;
					i++;
					break;
				}
				case 'd':
				{
					argv[i] = v8::Integer::New(isolate, static_cast<int32_t>(params[i + 1]));
					break;
				}
				case 'i':
				{
					argv[i] = v8::Integer::New(isolate, static_cast<uint32_t>(params[i + 1]));
					break;
				}
				case 'f':
				{
					argv[i] = v8::Number::New(isolate, amx_ctof(params[i + 1]));
					break;
				}
				}
			}


			v8::Local<v8::Function> function = listener.function.Get(isolate);
			v8::MaybeLocal<v8::Value> returnValue = function->Call(ctx, ctx->Global(), argc, argv);

			if (argc > 0) delete[] argv;

			if (eh.HasCaught())
			{
				v8::String::Utf8Value str(isolate, eh.Exception());
				v8::String::Utf8Value stack(isolate, eh.StackTrace(listener.context.Get(isolate)).ToLocalChecked());

				L_ERROR << "Exception thrown: " << *str << "\nstack:\n" << *stack;
			}
			else
			{
				int cppIntReturnValue = returnValue.ToLocalChecked()->Int32Value(ctx).ToChecked();
				if (retval != nullptr) *retval = static_cast<cell>(cppIntReturnValue);
			}
		}
	}

	void event::call_from_pawn_native(AMX* amx, cell* params, cell* retval)
	{
		std::vector<EventListener_t> copiedFunctionList = functionList;

		for (auto& listener : copiedFunctionList)
		{
			if (std::find(functionList.begin(), functionList.end(), listener) == functionList.end()) {
				continue;
			}

			v8::Isolate* isolate = listener.isolate;
			v8::Locker v8Locker(listener.isolate);
			v8::Isolate::Scope isolateScope(isolate);
			v8::HandleScope hs(listener.isolate);
			v8::Local<v8::Context> ctx = listener.context.Get(isolate);
			v8::Context::Scope cs(ctx);
			v8::TryCatch eh(listener.isolate);

			v8::Local<v8::Value>* argv = NULL;
			unsigned int argc = paramTypes.length();
			argv = new v8::Local<v8::Value>[argc];
			int paramOffset = 0;

			for (unsigned int i = 0; i < argc; i++)
			{
				switch (paramTypes[i])
				{
				case 's':
				{
					cell* maddr = NULL;
					int len = 0;
					char* sval;
					if (amx_GetAddr(amx, params[i + paramOffset + 1], &maddr) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get string address: " << name.c_str();
						return;
					}
					amx_StrLen(maddr, &len);
					sval = new char[len + 1];
					if (amx_GetString(sval, maddr, 0, len + 1) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get string address: " << name.c_str();
						return;
					}
					argv[i] = v8::String::NewFromUtf8(isolate, sval).ToLocalChecked();
					break;
				}
				case 'a':
				{
					cell* array = NULL;
					if (amx_GetAddr(amx, params[i + paramOffset + 1], &array) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get array address: " << name.c_str();
						return;
					}
					int size = static_cast<int>(*utils::get_amxaddr(amx, params[i + paramOffset + 2]));
					v8::Local<v8::Array> jsArray = v8::Array::New(isolate, size);
					for (int j = 0; j < size; j++)
					{
						jsArray->Set(ctx, j, v8::Integer::New(isolate, static_cast<uint32_t>(array[j])));
					}
					argv[i] = jsArray;
					paramOffset++;
					break;
				}
				case 'v':
				{
					cell* array = NULL;
					if (amx_GetAddr(amx, params[i + paramOffset + 1], &array) != AMX_ERR_NONE)
					{
						L_ERROR << "Can't get float array address: " << name.c_str();
						return;
					}

					int size = static_cast<int>(*utils::get_amxaddr(amx, params[i + paramOffset + 2]));
					v8::Local<v8::Array> jsArray = v8::Array::New(isolate, size);
					for (int j = 0; j < size; j++)
					{
						jsArray->Set(ctx, j, v8::Integer::New(isolate, amx_ctof(array[j])));
					}
					argv[i] = jsArray;
					paramOffset++;
					break;
				}
				case 'd':
				{
					argv[i] = v8::Integer::New(isolate, static_cast<int32_t>(*utils::get_amxaddr(amx, params[i + paramOffset + 1])));
					break;
				}
				case 'i':
				{
					argv[i] = v8::Integer::New(isolate, static_cast<uint32_t>(*utils::get_amxaddr(amx, params[i + paramOffset + 1])));
					break;
				}
				case 'f':
				{
					argv[i] = v8::Number::New(isolate, amx_ctof(*utils::get_amxaddr(amx, params[i + paramOffset + 1])));
					break;
				}
				}
			}

			v8::Local<v8::Function> function = listener.function.Get(listener.isolate);
			v8::MaybeLocal<v8::Value> returnValue = function->Call(ctx, ctx->Global(), argc, argv);

			if (argc > 0) delete[] argv;

			if (eh.HasCaught())
			{
				v8::String::Utf8Value str(listener.isolate, eh.Exception());
				v8::String::Utf8Value stack(listener.isolate, eh.StackTrace(listener.context.Get(listener.isolate)).ToLocalChecked());

				L_ERROR << "Exception thrown: " << *str << "\nstack:\n" << *stack << "\n";
			}
			else
			{
				int cppIntReturnValue = returnValue.ToLocalChecked()->Int32Value(ctx).ToChecked();
				if (retval != nullptr) *retval = static_cast<cell>(cppIntReturnValue);
			}
		}
	}
}