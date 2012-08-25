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

#include "Common.h"
#include "Log.h"
#include "Unit.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Vehicle.h"
#include "Util.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "MapManager.h"

VehicleInfo::VehicleInfo(Unit* owner, VehicleEntry const* entry) : TransportBase(owner),
    m_vehicleEntry(entry),
    m_creatureSeats(0),
    m_playerSeats(0)
{
    for (uint8 i = 0; i < MAX_VEHICLE_SEAT; ++i)
    {
        if (uint32 seatId = entry->m_seatID[i])
        {
            if (VehicleSeatEntry const* seatEntry = sVehicleSeatStore.LookupEntry(seatId))
            {
                m_vehicleSeats.insert(VehicleSeatMap::value_type(i, seatEntry));

                if (IsUsableSeatForCreature(seatEntry->m_flags))
                    m_creatureSeats |= 1 << i;

                if (IsUsableSeatForPlayer(seatEntry->m_flags))
                    m_playerSeats |= 1 << i;
            }
        }
    }
}

void VehicleInfo::Board(Unit* passenger, uint8 seat)
{
    MANGOS_ASSERT(passenger != NULL);

    DEBUG_LOG("VehicleInfo::Board: Try to board passenger %s to seat %u", passenger->GetObjectGuid().GetString().c_str(), seat);

    // This is called in Spell::CheckCast()
    // but check also here preventively! :)
    if (!CanBoard(passenger))
        return;

    uint8 possibleSeats = (passenger->GetTypeId() == TYPEID_PLAYER) ? m_playerSeats : m_creatureSeats;

    // Use the planned seat only if the seat is valid, possible to choose and empty
    if (seat >= MAX_VEHICLE_SEAT || ~possibleSeats & (1 << seat) || GetTakenSeatsMask() & (1 << seat))
        if (!GetUsableSeatFor(passenger, seat))
            return;

    VehicleSeatEntry const* seatEntry = GetSeatEntry(seat);

    MANGOS_ASSERT(seatEntry != NULL);

    // ToDo: Unboard passenger
    /*if (TransportInfo* transportInfo = passenger->GetTransportInfo())
    {
        WorldObject* transporter = transportInfo->GetTransport();

        // Must be a MO transporter
        MANGOS_ASSERT(transporter->GetObjectGuid().IsMOTransport());

        ((Transport*)transporter)->RemovePassenger(passenger);
    }*/

    DEBUG_LOG("VehicleInfo::Board: Board passenger: %s to seat %u", passenger->GetObjectGuid().GetString().c_str(), seat);

    // Calculate passengers local position
    float lx, ly, lz, lo;
    CalculateBoardingPositionOf(passenger->GetPositionX(), passenger->GetPositionY(), passenger->GetPositionZ(),
        passenger->GetOrientation(), lx, ly, lz, lo);

    TransportInfo* transportInfo = new TransportInfo(passenger, this, lx, ly, lz, lo, seat);

    // Insert our new passenger :D
    m_passengers.insert(PassengerMap::value_type(passenger, transportInfo));

    // The passenger needs fast access to transportInfo
    passenger->SetTransportInfo(transportInfo);

    // To players this is set automatically, somewhere
    passenger->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);

    if (passenger->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pPlayer = (Player*)passenger;
        pPlayer->RemovePet(PET_SAVE_AS_CURRENT);

        WorldPacket data(SMSG_ON_CANCEL_EXPECTED_RIDE_VEHICLE_AURA);
        pPlayer->GetSession()->SendPacket(&data);

        // SMSG_BREAK_TARGET

        pPlayer->GetCamera().SetView(m_owner);
    }

    if (!passenger->IsRooted())
        passenger->SetRoot(true);

    Movement::MoveSplineInit init(*passenger);
    init.MoveTo(0.0f, 0.0f, 0.0f); // ToDo: Set correct local coords
    // Sometimes Spline Type Normal with Spline Flag WalkMode instead
    // Other combinations are possible too...
    init.SetFacing(0.0f); // local orientation ? ToDo: Set proper orientation!
    init.SetBoardVehicle();
    init.Launch();

    if (passenger->GetTypeId() == TYPEID_PLAYER && seatEntry->m_flags & SEAT_FLAG_CAN_CONTROL)
    {
        Player* pPlayer = (Player*)passenger;
        Unit* pVehicle = (Unit*)m_owner;

        pPlayer->SetCharm(pVehicle);
        pVehicle->SetCharmerGuid(pPlayer->GetObjectGuid());

        pVehicle->addUnitState(UNIT_STAT_CONTROLLED);
        pVehicle->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);

        pPlayer->SetClientControl(pVehicle, 1);
        pPlayer->SetMover(pVehicle);

        // Unconfirmed and it is also not clear if this is a good solution... 
        if (m_owner->GetTypeId() == TYPEID_UNIT)
        {
            if (!pPlayer->HasMovementFlag(MOVEFLAG_WALK_MODE) && pVehicle->m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE))
            {
                ((Creature*)m_owner)->SetWalk(false);
            }
            else if (pPlayer->HasMovementFlag(MOVEFLAG_WALK_MODE) && !pVehicle->m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE))
            {
                ((Creature*)m_owner)->SetWalk(true);
            }
        }

        // ToDo: Possesscode needs a few improvements
        CharmInfo* charmInfo = pVehicle->InitCharmInfo(pVehicle);
        charmInfo->InitPossessCreateSpells();
        charmInfo->SetReactState(REACT_PASSIVE);

        pPlayer->PossessSpellInitialize();
    }
}

void VehicleInfo::Unboard(Unit* passenger)
{
    MANGOS_ASSERT(passenger != NULL);

    PassengerMap::const_iterator itr = m_passengers.find(passenger);

    if (itr == m_passengers.end())
        return;

    DEBUG_LOG("VehicleInfo::Unboard: passenger: %s", passenger->GetObjectGuid().GetString().c_str());

    VehicleSeatEntry const* seatEntry = GetSeatEntry(itr->second->GetTransportSeat());

    MANGOS_ASSERT(seatEntry != NULL);

    // Set passengers transportInfo to NULL
    passenger->SetTransportInfo(NULL);

    // Update movementInfo
    passenger->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);

    if (passenger->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pPlayer = (Player*)passenger;
        pPlayer->ResummonPetTemporaryUnSummonedIfAny();

        //SMSG_PET_DISMISS_SOUND
    }

    if (passenger->IsRooted())
        passenger->SetRoot(false);

    Movement::MoveSplineInit init(*passenger);
    // ToDo: Set proper unboard coordinates
    init.MoveTo(m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ());
    init.SetExitVehicle();
    init.Launch();

    if (passenger->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pPlayer = (Player*)passenger;

        if (seatEntry->m_flags & SEAT_FLAG_CAN_CONTROL)
        {
            Unit* pVehicle = (Unit*)m_owner;

            pPlayer->SetCharm(NULL);
            pVehicle->SetCharmerGuid(ObjectGuid());

            pPlayer->SetClientControl(pVehicle, 0);
            pPlayer->SetMover(NULL);

            pVehicle->clearUnitState(UNIT_STAT_CONTROLLED);
            pVehicle->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);

            pPlayer->RemovePetActionBar();
        }

        // must be called after movement control unapplying
        pPlayer->GetCamera().ResetView();
    }

    // Delete transportInfo
    delete itr->second;

    // Unboard finally :)
    m_passengers.erase(itr);
}

VehicleSeatEntry const* VehicleInfo::GetSeatEntry(uint8 seat) const
{
    VehicleSeatMap::const_iterator itr = m_vehicleSeats.find(seat);

    // Seat not available...
    if (itr == m_vehicleSeats.end())
        return NULL;

    return itr->second;
}

bool VehicleInfo::CanBoard(Unit* passenger)
{
    if (!passenger)
        return false;

    // Check if we have at least one empty seat
    if (!GetEmptySeats())
        return false;

    // Passenger is already boarded
    if (m_passengers.find(passenger) != m_passengers.end())
        return false;

    // Check for empty player seats
    if (passenger->GetTypeId() == TYPEID_PLAYER)
        return GetEmptySeatsMask() & m_playerSeats;

    // Check for empty creature seats
    return GetEmptySeatsMask() & m_creatureSeats;
}

bool VehicleInfo::GetUsableSeatFor(Unit* passenger, uint8& seat)
{
    uint8 emptySeats = GetEmptySeatsMask();
    uint8 possibleSeats = (passenger->GetTypeId() == TYPEID_PLAYER) ? m_playerSeats : m_creatureSeats;

    for (uint8 i = 1, seat = 0; seat < MAX_VEHICLE_SEAT; i <<= 1, ++seat)
        if (emptySeats & i && possibleSeats & i)
            return true;

    return false;
}

uint8 VehicleInfo::GetTakenSeatsMask()
{
    uint8 takenSeatsMask = 0;

    for (PassengerMap::const_iterator itr = m_passengers.begin(); itr != m_passengers.end(); ++itr)
        takenSeatsMask |= 1 << itr->second->GetTransportSeat();

    return takenSeatsMask;
}

void VehicleInfo::CalculateBoardingPositionOf(float gx, float gy, float gz, float go, float &lx, float &ly, float &lz, float &lo)
{
    NormalizeRotatedPosition(gx - m_owner->GetPositionX(), gy - m_owner->GetPositionY(), lx, ly);

    lz = gz - m_owner->GetPositionZ();
    lo = MapManager::NormalizeOrientation(go - m_owner->GetOrientation());
}
