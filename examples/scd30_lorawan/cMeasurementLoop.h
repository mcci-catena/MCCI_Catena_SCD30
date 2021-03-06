/*

Module:	cMeasurementLoop.h

Function:
	Measurement loop object for SCD30 LoRaWAN demo

Copyright and License:
	This file copyright (C) 2020 by

		MCCI Corporation
		3520 Krums Corners Road
		Ithaca, NY  14850

	See accompanying LICENSE file for copyright and license information.

Author:
	Terry Moore, MCCI Corporation	October 2020

*/

#ifndef _cMeasurementLoop_h_
#define _cMeasurementLoop_h_	/* prevent multiple includes */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include <Catena_FSM.h>
#include <Catena_Led.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Catena_Timer.h>
#include <Catena_TxBuffer.h>
#include <MCCI_Catena_SCD30.h>
#include <mcciadk_baselib.h>
#include <stdlib.h>

#include <cstdint>

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

class cMeasurementLoop : public McciCatena::cPollableObject
    {
public:
    // constructor
    cMeasurementLoop(
            McciCatenaScd30::cSCD30& scd30
            )
        : m_Scd(scd30)
        {};

    // neither copyable nor movable
    cMeasurementLoop(const cMeasurementLoop&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&) = delete;
    cMeasurementLoop(const cMeasurementLoop&&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&&) = delete;

    enum class State : std::uint8_t
        {
        stNoChange = 0, // this name must be present: indicates "no change of state"
        stInitial,      // this name must be present: it's the starting state.
        stInactive,     // parked; not doing anything.
        stSleeping,     // active; sleeping between measurements
        stWake,      	// wake up any sensors that need to be awakend
        stMeasure,   	// make the measurements
        stSleepSensor,  // sleep any sensors that need to be put to sleep
        stTransmit,     // transmit data

        stFinal,        // this name must be present, it's the terminal state.
        };

    static constexpr const char *getStateName(State s)
        {
        switch (s)
            {
        case State::stNoChange: return "stNoChange";
        case State::stInitial: return "stInitial";
        case State::stInactive: return "stInactive";
        case State::stSleeping: return "stSleeping";
        case State::stWake: return "stWake";
        case State::stMeasure: return "stMeasure";
        case State::stSleepSensor: return "stSleepSensor";
        case State::stTransmit: return "stTransmit";
        case State::stFinal: return "stFinal";
        default: return "<<unknown>>";
            }
        }

    static constexpr uint8_t kUplinkPort = 1;
    static constexpr uint8_t kMessageFormat = 0x1E;

    enum class Flags : uint8_t
            {
            Vbat = 1 << 0,
            Vcc = 1 << 1,
            Boot = 1 << 2,
            TH = 1 << 3,        // temperature (int16, 0.005 deg C),
                                // rh (uint16, 0xFFFF = 100%)
            CO2ppm = 1 << 4,    // CO2 PPM, uflt16
            };

    static constexpr size_t kTxBufferSize = 36;
    using TxBuffer_t = McciCatena::AbstractTxBuffer_t<kTxBufferSize>;

    // initialize measurement FSM.
    void begin();
    void end();
    virtual void poll() override;

    // request that the measurement loop be active/inactive
    void requestActive(bool fEnable);

private:
    // evaluate the control FSM.
    State fsmDispatch(State currentState, bool fEntry);

    // set the timer
    void setTimer(std::uint32_t ms)
        {
        this->m_timer_start = millis();
        this->m_timer_delay = ms;
        this->m_fTimerActive = true;
        this->m_fTimerEvent = false;
        }
    void clearTimer()
        {
        this->m_fTimerActive = false;
        this->m_fTimerEvent = false;
        }
    bool timedOut()
        {
        bool result = this->m_fTimerEvent;
        this->m_fTimerEvent = false;
        return result;
        }

    // sleep handling
    void sleep();
    // can we enter deep sleep?
    bool checkDeepSleep();
    void doSleepAlert(bool fDeepSleep);
    void doDeepSleep();
    void deepSleepPrepare();
    void deepSleepRecovery();

    void fillTxBuffer(TxBuffer_t &b);
    void startTransmission(TxBuffer_t &b);
    void sendBufferDone(bool fSuccess);
    bool txComplete()
        {
        return this->m_txcomplete;
        }
    void updateTxCycleTime();

    // instance data
    McciCatena::cFSM <cMeasurementLoop, State>
                        m_fsm;
    McciCatenaScd30::cSCD30&    m_Scd;

    // true if object is registered for polling.
    bool                m_registered : 1;
    // true if object is running.
    bool                m_running : 1;
    // true to request exit
    bool                m_exit : 1;
    // true if in active uplink mode, false otehrwise.
    bool                m_active : 1;

    // set true to request transition to active uplink mode; cleared by FSM
    bool                m_rqActive : 1;
    // set true to request transition to inactive uplink mode; cleared by FSM
    bool                m_rqInactive : 1;

    // set true if measurement is valid
    bool                m_measurement_valid: 1;

    // set true if event timer times out
    bool                m_fTimerEvent : 1;
    // set true while evenet timer is active.
    bool                m_fTimerActive : 1;
    // set true if CO2 (SCD) is present
    bool                m_fSCD : 1;
    // set true while a transmit is pending.
    bool                m_txpending : 1;
    // set true when a transmit completes.
    bool                m_txcomplete : 1;
    // set true when a transmit complete with an error.
    bool                m_txerr : 1;
    // set true when we've printed how we plan to sleep
    bool                m_fPrintedSleeping : 1;

    // for simple internal timer.
    std::uint32_t           m_timer_start;
    std::uint32_t           m_timer_delay;
    };

static constexpr cMeasurementLoop::Flags operator| (const cMeasurementLoop::Flags lhs, const cMeasurementLoop::Flags rhs)
        {
        return cMeasurementLoop::Flags(uint8_t(lhs) | uint8_t(rhs));
        };

static cMeasurementLoop::Flags operator|= (cMeasurementLoop::Flags &lhs, const cMeasurementLoop::Flags &rhs)
        {
        lhs = lhs | rhs;
        return lhs;
        };

#endif /* _cMeasurementLoop_h_ */
