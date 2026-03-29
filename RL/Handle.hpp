#pragma once

namespace aita
{
	template <typename T, T Invalid, auto CloseFn>
	class Handle
	{
	public:
		Handle() = default;

		explicit Handle(T handle) :
			_handle(handle)
		{
		}

		~Handle()
		{
			if (isValid())
			{
				CloseFn(_handle);
			}
		}

		Handle(const Handle&) = delete;
		Handle& operator = (const Handle&) = delete;
		Handle(Handle&& other) = delete;
		Handle& operator = (Handle&& other) = delete;

		operator T() const
		{
			return _handle;
		}

		T* addressOf()
		{
			return &_handle;
		}

		bool isValid() const
		{
			if constexpr (std::is_pointer_v<T>)
			{
				return _handle != nullptr && _handle != Invalid;
			}
			else
			{
				return _handle != Invalid;
			}
		}

		void reset(T handle = Invalid)
		{
			if (isValid())
			{
				CloseFn(_handle);
			}

			_handle = handle;
		}

	private:
		T _handle = Invalid;
	};

#ifdef WIN32
	using WinHandle = Handle<HANDLE, INVALID_HANDLE_VALUE, CloseHandle>;
#else
	using PosixHandle = Handle<int, -1, close>;
#endif
}