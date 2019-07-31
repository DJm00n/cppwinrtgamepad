#include "pch.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <mutex>
#include <chrono>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Gaming::Input;

static std::wstring IntToHexString(uint16_t in)
{
    std::wstringstream sstream;
    sstream << L"0x" << std::hex << std::setw(4) << std::setfill(L'0') << in;
    return sstream.str();
};

class GamepadManager
{
    struct GamepadWithButtonState
    {
        Gamepad gamepad;
        std::wstring name;
        bool buttonAWasPressedLastFrame = false;
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

        std::wstring name(L"Generic Xbox Gamepad");

        RawGameController rawController = RawGameController::FromGameController(gamepad);
        if (rawController)
        {
            name = rawController.DisplayName().c_str();
            name.append(L" (VID:")
                .append(IntToHexString(rawController.HardwareVendorId()))
                .append(L" PID:")
                .append(IntToHexString(rawController.HardwareProductId()))
                .append(L")");
        }

        m_gamepads.emplace_back({ gamepad, name, false });

        std::wcout << "Connected: " << name << std::endl;
    }

    void OnGamepadRemoved(IInspectable const& /* sender */, Gamepad const& gamepad)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_gamepads.erase(std::remove_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
        {
            if (gamepadWithState.gamepad != gamepad)
                return false;

            std::wcout << "Disconnected: " << gamepadWithState.name << std::endl;

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

            bool buttonDownThisUpdate = ((reading.Buttons & GamepadButtons::A) == GamepadButtons::A);
            if (buttonDownThisUpdate && !gamepadWithButtonState.buttonAWasPressedLastFrame)
            {
                std::wcout << "Button A pressed on: " << gamepadWithButtonState.name << std::endl;
            }
            gamepadWithButtonState.buttonAWasPressedLastFrame = buttonDownThisUpdate;
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
        std::this_thread::sleep_for(200ms);
    }
}