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
        for (GamepadWithButtonState& gamepadWithButtonState : m_gamepads)
        {
            GamepadReading reading = gamepadWithButtonState.gamepad.GetCurrentReading();

            if (reading.Timestamp != gamepadWithButtonState.timestamp)
            {
                bool pressedA = ((reading.Buttons & GamepadButtons::A) == GamepadButtons::A);
                std::cout << gamepadWithButtonState.name << ": " << "Timestamp=" << reading.Timestamp << ", PressedA=" << pressedA << std::endl;
                gamepadWithButtonState.timestamp = reading.Timestamp;
            }
        }
    }
};

int main()
{
    using namespace std::chrono_literals;

    winrt::init_apartment();

    GamepadManager gamepads;

    while (true)
    {
        gamepads.Update();
        std::this_thread::sleep_for(100ms);
    }
}