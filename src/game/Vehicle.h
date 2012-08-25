/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOSSERVER_VEHICLE_H
#define MANGOSSERVER_VEHICLE_H

#include "Common.h"
#include "ObjectGuid.h"
#include "TransportSystem.h"
#include "SharedDefines.h"

class Unit;

struct VehicleEntry;
struct VehicleSeatEntry;

typedef UNORDERED_MAP<uint8, VehicleSeatEntry const*> VehicleSeatMap;

class VehicleInfo : public TransportBase
{
    public:
        explicit VehicleInfo(Unit* owner, VehicleEntry const* entry);

        void Board(Unit* passenger, uint8 seat = 0);
        void Unboard(Unit* passenger);

        VehicleEntry const* GetEntry() const { return m_vehicleEntry; }
        VehicleSeatEntry const* GetSeatEntry(uint8 seat) const;

        bool CanBoard(Unit* passenger);
        bool GetUsableSeatFor(Unit* passenger, uint8& seat);

        bool IsUsableSeatForPlayer(uint32 seatFlags) { return seatFlags & SEAT_FLAG_USABLE; }
        bool IsUsableSeatForCreature(uint32 seatFlags) { return true; /* return !IsUsableSeatForPlayer(seatFlags); */ }

        uint8 GetTakenSeatsMask();
        uint8 GetEmptySeatsMask() { return ~GetTakenSeatsMask(); }
        uint8 GetEmptySeats() { return m_vehicleSeats.size() - m_passengers.size(); }

    private:
        void CalculateBoardingPositionOf(float gx, float gy, float gz, float go, float &lx, float &ly, float &lz, float &lo);

        VehicleEntry const* m_vehicleEntry;
        VehicleSeatMap m_vehicleSeats;
        uint8 m_creatureSeats;
        uint8 m_playerSeats;
};

#endif
