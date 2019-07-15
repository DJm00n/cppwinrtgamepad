#include "pch.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <mutex>
#include <chrono>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Gaming::Input;

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

    winrt::event_token m_gamepadAddedEventToken;
    winrt::event_token m_gamepadRemovedEventToken;

public:
    GamepadManager()
    {
        m_gamepadAddedEventToken = Gamepad::GamepadAdded(bind(&GamepadManager::OnGamepadAdded, this, std::placeholders::_1, std::placeholders::_2));
        m_gamepadRemovedEventToken = Gamepad::GamepadRemoved(bind(&GamepadManager::OnGamepadRemoved, this, std::placeholders::_1, std::placeholders::_2));

        for (Gamepad const& gamepad : Gamepad::Gamepads())
        {
            OnGamepadAdded(nullptr, gamepad);
        }
    }

    ~GamepadManager()
    {
        Gamepad::GamepadAdded(m_gamepadAddedEventToken);
        Gamepad::GamepadRemoved(m_gamepadRemovedEventToken);
    }

    void OnGamepadAdded(winrt::Windows::Foundation::IInspectable, Gamepad const& args)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        auto it = std::find_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
        {
            return gamepadWithState.gamepad == args;
        });

        // This gamepad is already in the list.
        if (it != m_gamepads.end())
            return;

        auto rawController = RawGameController::FromGameController(args);
        uint16_t vid = rawController.HardwareVendorId();
        uint16_t pid = rawController.HardwareProductId();
        winrt::hstring name = rawController.DisplayName();

        GamepadWithButtonState newGamepad = { args, name.c_str(), false };
        m_gamepads.push_back(newGamepad);

        std::wcout << "Connected: " << newGamepad.name;
        std::wcout << " ("
            << "0x" << std::hex << std::setw(4) << std::setfill(L'0') << vid << ":"
            << "0x" << std::hex << std::setw(4) << std::setfill(L'0') << pid << ")" << std::endl;
    }

    void OnGamepadRemoved(winrt::Windows::Foundation::IInspectable, Gamepad const& args)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_gamepads.erase(std::remove_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
        {
            if (gamepadWithState.gamepad != args)
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