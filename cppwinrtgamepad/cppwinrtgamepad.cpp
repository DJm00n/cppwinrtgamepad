#include "pch.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <random>

template <class Resolution = std::chrono::milliseconds>
class ExecutionTimer {
public:
    using Clock = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
        std::chrono::high_resolution_clock,
        std::chrono::steady_clock>;

private:
    const Clock::time_point mStart = Clock::now();

public:
    ExecutionTimer() = default;
    ~ExecutionTimer() {
        const auto end = Clock::now();
        std::ostringstream strStream;
        strStream << "Elapsed: "
            << std::chrono::duration_cast<Resolution>(end - mStart).count() << " us";
        std::cout << strStream.str() << std::endl;
    }

    inline void stop() {
        const auto end = Clock::now();
        std::ostringstream strStream;
        strStream << "Elapsed: "
            << std::chrono::duration_cast<Resolution>(end - mStart).count() << " us";
        std::cout << strStream.str() << std::endl;
    }
};

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Gaming::Input;

template<typename T> std::string to_hex_string(const T& t)
{
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << t;
    return ss.str();
}

std::string format_vid_pid(uint16_t vid, uint16_t pid)
{
    std::stringstream ss;
    ss << "(VID:" << to_hex_string(vid) << " PID:" << to_hex_string(pid) << ")";
    return ss.str();
}

class GamepadManager
{
    struct GamepadWithButtonState
    {
        Gamepad gamepad;
        std::string name;
        uint64_t timestamp;
        bool pressedA;
        uint16_t motorNum;
    };

    std::vector<GamepadWithButtonState> m_gamepads;
    std::mutex m_mutex;

    Gamepad::GamepadAdded_revoker m_AddedRevoker;
    Gamepad::GamepadRemoved_revoker m_RemovedRevoker;

public:
    GamepadManager()
    {
        m_AddedRevoker = Gamepad::GamepadAdded(winrt::auto_revoke, std::bind(&GamepadManager::OnGamepadAdded, this, std::placeholders::_1, std::placeholders::_2));
        m_RemovedRevoker = Gamepad::GamepadRemoved(winrt::auto_revoke, std::bind(&GamepadManager::OnGamepadRemoved, this, std::placeholders::_1, std::placeholders::_2));

        Collections::IVectorView<Gamepad> gamepads = Gamepad::Gamepads();
        for (const Gamepad& gamepad : gamepads)
        {
            OnGamepadAdded(nullptr, gamepad);
        }
    }

    void OnGamepadAdded(IInspectable const& /* sender */, Gamepad const& gamepad)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        auto it = std::find_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
        {
            return gamepadWithState.gamepad == gamepad;
        });

        // This gamepad is already in the list.
        if (it != m_gamepads.end())
            return;

        std::string name;
        std::string vidpid;

        RawGameController rawController = RawGameController::FromGameController(gamepad);
        if (rawController)
        {
            name = winrt::to_string(rawController.DisplayName());
            vidpid = format_vid_pid(rawController.HardwareVendorId(), rawController.HardwareProductId());
        }
        else
        {
            name = "Generic Xbox Gamepad";
            vidpid = format_vid_pid(0, 0);
        }

        m_gamepads.emplace_back(GamepadWithButtonState { gamepad, name, 0 });

        std::cout << "Connected: " << name << " " << vidpid <<  std::endl;
    }

    void OnGamepadRemoved(IInspectable const& /* sender */, Gamepad const& gamepad)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_gamepads.erase(std::remove_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
        {
            if (gamepadWithState.gamepad != gamepad)
                return false;

            std::cout << "Disconnected: " << gamepadWithState.name << std::endl;

            return true;
        }), m_gamepads.end());
    }

    void Update()
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        // Check for new input state since the last frame.
        for (GamepadWithButtonState& pad : m_gamepads)
        {
            GamepadReading reading = pad.gamepad.GetCurrentReading();

            if (reading.Timestamp != pad.timestamp)
            {
                bool pressedA = ((reading.Buttons & GamepadButtons::A) == GamepadButtons::A);
                //std::cout << pad.name << ": " << "Timestamp=" << reading.Timestamp << ", PressedA=" << pressedA << std::endl;
                if (pressedA != pad.pressedA)
                {
                    if (pressedA)
                    {
                        pad.motorNum = pad.motorNum == 4 ? 0 : pad.motorNum + 1;
                        std::cout << "Motor num=" << pad.motorNum << std::endl;
                    }
                    pad.pressedA = pressedA;
                }
                pad.timestamp = reading.Timestamp;
            }

            GamepadVibration vibration {};

            switch (pad.motorNum)
            {
            case 1:
                vibration.LeftMotor = GetSinValue();
                break;
            case 2:
                vibration.RightMotor = GetSinValue();
                break;
            case 3:
                vibration.LeftTrigger = GetSinValue();
                break;
            case 4:
                vibration.RightTrigger = GetSinValue();
                break;
            }

            {
                std::cout << pad.name << ": put_Vibration: ";
                ExecutionTimer<std::chrono::microseconds> timer;
                pad.gamepad.Vibration(vibration);
            }
        }
    }

    static double GetSinValue()
    {
        static const double doublePi = std::acos(-1) * 2;
        static const double amplitude = 0.5;
        static const double translate = 0.5;
        static const double frequency = 0.5;
        static const double phase = 0.0;

        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        auto epoch = now_ms.time_since_epoch();

        return amplitude * sin(doublePi * frequency * (epoch.count() / 1000.0) + phase) + translate;
    }
};

std::atomic_bool stopGamepadThread = false;

int roll_die() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distrib(50, 100);

    return distrib(gen);
}

void GamepadThread()
{
    using namespace std::chrono_literals;

    winrt::init_apartment();

    GamepadManager gamepads;

    while (true)
    {
        if (stopGamepadThread)
            return;

        gamepads.Update();
        auto sleep = std::chrono::milliseconds(roll_die());
        std::cout << "Sleeping for " << sleep.count() << "ms.\n";
        std::this_thread::sleep_for(sleep);

    }
}