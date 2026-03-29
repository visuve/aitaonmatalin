#include "Keyboard.hpp"
#include "Logger.hpp"
#include "Handle.hpp"

namespace aita
{
	constexpr char KeyChars[] = { 'L', 'R', 'J' };

	Key keyFromIndex(int64_t index)
	{
		switch (index)
		{
			case 0:
				return Key::Left;
			case 1:
				return Key::Right;
			case 2:
				return Key::Jump;
		}

		throw std::invalid_argument("Invalid index");
	}

#ifdef WIN32
	constexpr DWORD KeyDown = 0x0000;
	constexpr DWORD KeyExtended = 0x0001;
	constexpr DWORD KeyUp = 0x0002;

	BYTE toVirtualKey(Key key)
	{
		switch (key)
		{
		case Key::Left: return VK_LEFT;
		case Key::Right: return VK_RIGHT;
		case Key::Jump: return VK_SPACE;
		}

		throw std::invalid_argument("Invalid key");
	}
#else
	uint16_t KeyDown = 1;
	uint16_t KeyUp = 0;

	uint16_t toEvdevCode(Key key)
	{
		switch (key)
		{
			case Key::Left: return KEY_LEFT;
			case Key::Right: return KEY_RIGHT;
			case Key::Jump: return KEY_SPACE;
		}

		throw std::invalid_argument("Invalid key");
	}

	class VirtualInputDevice
	{
	public:
		static VirtualInputDevice& instance()
		{
			static VirtualInputDevice instance;
			return instance;
		}

		void sendEvent(uint16_t type, uint16_t code, int32_t val) const
		{
			struct input_event ie = {};
			ie.type = type;
			ie.code = code;
			ie.value = val;
			gettimeofday(&ie.time, nullptr);

			if (write(_deviceDescriptor, &ie, sizeof(ie)) < 0)
			{
				LOGE("Failed to send event: type={}, code={}", type, code);
			}
		}

	private:
		VirtualInputDevice() :
			_deviceDescriptor(open("/dev/uinput", O_WRONLY | O_NONBLOCK))
		{
			if (!_deviceDescriptor.isValid())
			{
				throw std::system_error(errno, std::generic_category(), "Failed to open /dev/uinput");
			}

			ioctl(_deviceDescriptor, UI_SET_EVBIT, EV_KEY);
			ioctl(_deviceDescriptor, UI_SET_KEYBIT, KEY_LEFT);
			ioctl(_deviceDescriptor, UI_SET_KEYBIT, KEY_RIGHT);
			ioctl(_deviceDescriptor, UI_SET_KEYBIT, KEY_SPACE);

			struct uinput_setup usetup = {};
			usetup.id.bustype = BUS_VIRTUAL;
			usetup.id.vendor = 0x1D6B; // Linux Foundation
			usetup.id.product = 0x0104; // Custom/Generic Software Input
			usetup.id.version = 1;
			strcpy(usetup.name, "aita_virtual_keyboard");

			ioctl(_deviceDescriptor, UI_DEV_SETUP, &usetup);
			ioctl(_deviceDescriptor, UI_DEV_CREATE);

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		~VirtualInputDevice()
		{
			if (_deviceDescriptor.isValid())
			{
				ioctl(_deviceDescriptor, UI_DEV_DESTROY);
			}
		}

		VirtualInputDevice(const VirtualInputDevice&) = delete;
		VirtualInputDevice& operator = (const VirtualInputDevice&) = delete;
		VirtualInputDevice(VirtualInputDevice&&) = delete;
		VirtualInputDevice& operator = (VirtualInputDevice&&) = delete;

		PosixHandle _deviceDescriptor;
	};
#endif

	KeyPress::KeyPress(Key key, std::chrono::milliseconds from, std::chrono::milliseconds to) :
		key(key),
		from(from),
		to(to)
	{
	}

	void KeyPress::execute(std::stop_source stop_source, std::chrono::steady_clock::time_point startTime)
	{
		std::stop_token token = stop_source.get_token();

		const auto offset = std::chrono::steady_clock::now() - startTime;
		const auto delay = from - offset;
		const auto duration = to - from;

		const auto wait = [&](std::chrono::nanoseconds waitTime)
		{
			if (waitTime <= std::chrono::milliseconds(1))
			{
				return true;
			}

			std::mutex mtx;
			std::unique_lock<std::mutex> lock(mtx);
			std::condition_variable_any cv;

			cv.wait_for(lock, token, waitTime, []
			{
				return false;
			});

			return !token.stop_requested();
		};

#ifdef WIN32
	 	const BYTE virtualKey = toVirtualKey(key);
		const UINT scan = static_cast<BYTE>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));

		if (!wait(delay))
		{
			return;
		}

		keybd_event(virtualKey, scan, KeyDown | KeyExtended, 0);
		wait(duration);
		keybd_event(virtualKey, scan, KeyUp | KeyExtended, 0);
#else
		const uint16_t evdevCode = toEvdevCode(key);

		if (!wait(delay))
		{
			return;
		}

		const auto& device = VirtualInputDevice::instance();

		device.sendEvent(EV_KEY, evdevCode, KeyDown);
		device.sendEvent(EV_SYN, SYN_REPORT, 0);

		wait(duration);

		device.sendEvent(EV_KEY, evdevCode, KeyUp);
		device.sendEvent(EV_SYN, SYN_REPORT, 0);
#endif
		LOGD("({}, {}, {}) executed", KeyChars[static_cast<size_t>(key)], from, to);
	}

	Keyboard::~Keyboard()
	{
		_stopSource.request_stop();
	}

	Keyboard& Keyboard::operator<<(KeyPress&& key)
	{
		_keys.emplace_back(key);
		return *this;
	}

	void Keyboard::sendKeys()
	{
		std::string message;
		auto startTime = std::chrono::steady_clock::now();

		for (KeyPress& kp : _keys)
		{
			if (!message.empty())
			{
				message += ", ";
			}

			message += std::format("({}, {}, {})", KeyChars[static_cast<size_t>(kp.key)], kp.from, kp.to);

			_threads.emplace_back(&KeyPress::execute, &kp, _stopSource, startTime);
		}

		LOGI("Executing: {}", message.empty() ? "None" : message);
	}

	void Keyboard::wait()
	{
		for (std::jthread& thread : _threads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}
	}
}
