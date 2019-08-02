#include "pch.h"

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <mutex>
#include <chrono>
#include <codecvt>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Gaming::Input;

std::string WStringToString(const std::wstring& wstr)
{
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    return converter.to_bytes(wstr);
}

std::string IntToHexString(uint16_t in)
{
    std::stringstream sstream;
    sstream << "0x" << std::hex << std::setw(4) << std::setfill('0') << in;

    return sstream.str();
};

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
        m_AddedRevoker = Gamepad::GamepadAdded(winrt::auto_revoke, bind(&GamepadManager::OnGamepadAdded, this, std::placeholders::_1, std::placeholders::_2));
        m_RemovedRevoker = Gamepad::GamepadRemoved(winrt::auto_revoke, bind(&GamepadManager::OnGamepadRemoved, this, std::placeholders::_1, std::placeholders::_2));

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

        std::string name("Generic Xbox Gamepad");
        std::string vidpid("(VID:0x0000 PID:0x0000)");

        RawGameController rawController = RawGameController::FromGameController(gamepad);
        if (rawController)
        {
            name = WStringToString(rawController.DisplayName().c_str());

            vidpid.clear();
            vidpid.append("(VID:")
                .append(IntToHexString(rawController.HardwareVendorId()))
                .append(" PID:")
                .append(IntToHexString(rawController.HardwareProductId()))
                .append(")");
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