#include "Screens.h"
#include <Arduino.h>
#include <U8g2lib.h>

Screens::Screens(U8G2 &u8g2, int numofscreens, unsigned long updateInterval, unsigned long screenTimeout)
{
    _u8g2 = u8g2;
    _currentScreen = 0;
    _numofscreens = numofscreens;
    _screenTimeout = screenTimeout;
    _updateInterval = updateInterval;
}

void Screens::setup()
{
    _u8g2.begin();

    //u8g2.setFont(u8g2_font_6x10_tf); // set the target font to calculate the pixel width
    //width = u8g2.getUTF8Width(text); // calculate the pixel width of the text
    //
    //u8g2.setFont(u8g2_font_unifont_t_symbols); // set the target font
    //width2 = u8g2.getUTF8Width(text2);         // calculate the pixel width of the text
    //
    _u8g2.setFontMode(0); // enable transparent mode, which is faster
    //
    _u8g2.setFontPosTop();
    _u8g2.setFontDirection(0);
    //
    _u8g2.setContrast(255);

    reset();
}

uint8_t Screens::currentScreen()
{
    return _currentScreen;
}

int Screens::count()
{
    return _numofscreens;
}

void Screens::loop()
{
    if ((millis() - _lastScreenActivation) >= _screenTimeout)
    {
        powerSave(true);
    }
    else
    {
        if ((millis() - _lastScreenUpdate) >= _updateInterval)
        {
            _needRefresh = true;
            _lastScreenUpdate = millis();
        }
    }
}

bool Screens::needRefresh()
{
    if (_modalMessageActive) // prevent update in loop if model Message is active
    {
        return false;
    }
    else
    {
        bool tmp = _needRefresh;
        _needRefresh = false;
        return tmp;
    }
}

void Screens::nextScreen()
{
    if (!_displayPowerSaving) // dont switch screen, if last display was in power saving mode
    {
        _currentScreen += 1;
    }

    if (_currentScreen > _numofscreens || _currentScreen == 0)
    {
        _currentScreen = 1;
    }
    _modalMessageActive = false;
    _needRefresh = true;
    powerSave(false);
    _lastScreenActivation = millis();
}

void Screens::showScreen(int screenNumber)
{
    if (screenNumber <= _numofscreens)
    {
        _currentScreen = screenNumber;
        _needRefresh = true;
        _modalMessageActive = false;
        powerSave(false);
        _lastScreenActivation = millis();
    }
}

void Screens::powerSave(bool activatePowerSave, bool force /* = false */)
{

    if (activatePowerSave && (!_displayPowerSaving || force)) // request to activate power save and display is not power saving mode
    {
        _u8g2.setPowerSave(1);
        _displayPowerSaving = true;
        _modalMessageActive = false;
    }
    else if (!activatePowerSave && (_displayPowerSaving || force)) // request to deactivate power save and display is power saving mode
    {
        _u8g2.setPowerSave(0);
        _displayPowerSaving = false;
    }
}

void Screens::reset()
{
    _currentScreen = 0;
    powerSave(true, true);
}

void Screens::displayMsg(const char *text, const char *text2 /* = "" */, const char *text3 /* = "" */, const char *text4 /* = "" */, const char *text5 /* = "" */)
{

    _u8g2.clearBuffer();                 // clear the internal memory
    _u8g2.setFont(u8g2_font_helvB08_tf); // choose a suitable font
    snprintf(_buff, sizeof(_buff), "%s (%i/%i)", "RoombaESP         ", _currentScreen, _numofscreens);
    //u8g2_uint_t width = _u8g2.getUTF8Width(_buff);
    //u8g2_uint_t offset = (_u8g2.getDisplayWidth() - width) / 2;
    //_u8g2.drawStr(offset, 2, _buff);     // write something to the internal memory
    if (_modalMessageActive)
    {
        _u8g2.drawStr(0, 2, "RoombaESP");
    }
    else
    {
        _u8g2.drawStr(0, 2, _buff);
    }
    _u8g2.setFont(u8g2_font_helvR08_tf); // choose a suitable font
    _u8g2.drawStr(0, 15, text);          // write something to the internal memory
    _u8g2.drawStr(0, 25, text2);         // write something to the internal memory
    _u8g2.drawStr(0, 35, text3);         // write something to the internal memory
    _u8g2.drawStr(0, 45, text4);         // write something to the internal memory
    _u8g2.drawStr(0, 55, text5);         // write something to the internal memory
    _u8g2.sendBuffer();                  // transfer internal memory to the display
}

void Screens::displayMsgForce(const char *text, const char *text2 /* = "" */, const char *text3 /* = "" */, const char *text4 /* = "" */, const char *text5 /* = "" */)
{
    _modalMessageActive = true;
    powerSave(false);
    _lastScreenActivation = millis();
    displayMsg(text, text2, text3, text4, text5);
}