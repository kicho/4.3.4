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
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"

void WorldSession::HandleDismissControlledVehicleOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received CMSG_DISMISS_CONTROLLED_VEHICLE");
    recvPacket.hexlike();

    ObjectGuid guid;
    MovementInfo movementInfo;

    recvPacket >> guid.ReadAsPacked();
    recvPacket >> movementInfo;

    Unit* vehicle = _player->GetMap()->GetUnit(guid);

    if (!vehicle || !vehicle->IsVehicle())
        return;

    // Overwrite movementInfo
    vehicle->m_movementInfo = movementInfo;

    // Remove Vehicle Control Aura
    vehicle->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, _player->GetObjectGuid());
}

void WorldSession::HandleRequestVehicleExitOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received CMSG_REQUEST_VEHICLE_EXIT");
    recvPacket.hexlike();

    TransportInfo* transportInfo = _player->GetTransportInfo();

    if (!transportInfo || !transportInfo->IsOnVehicle())
        return;

    ((Unit*)transportInfo->GetTransport())->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, _player->GetObjectGuid());
}

void WorldSession::HandleRequestVehicleSwitchSeatOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received CMSG_REQUEST_VEHICLE_SWITCH_SEAT");
    recvPacket.hexlike();

    ObjectGuid srcVehicleGuid;
    uint8 seatId;

    recvPacket >> srcVehicleGuid.ReadAsPacked();
    recvPacket >> seatId;

    Unit* srcVehicle = _player->GetMap()->GetUnit(srcVehicleGuid);

    if (!srcVehicle || !srcVehicle->IsVehicle())
        return;

    srcVehicle->GetVehicleInfo()->Unboard(_player);
    srcVehicle->GetVehicleInfo()->Board(_player, seatId);
}

void WorldSession::HandleChangeSeatsOnControlledVehicleOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received CMSG_CHANGE_SEATS_ON_CONTROLLED_VEHICLE");
    recvPacket.hexlike();

    ObjectGuid srcVehicleGuid;
    MovementInfo movementInfo;
    ObjectGuid destVehicleGuid;
    uint8 seatId;

    recvPacket >> srcVehicleGuid.ReadAsPacked();
    recvPacket >> movementInfo;
    recvPacket >> destVehicleGuid.ReadAsPacked();
    recvPacket >> seatId;

    Unit* srcVehicle = _player->GetMap()->GetUnit(srcVehicleGuid);

    if (!srcVehicle || !srcVehicle->IsVehicle())
        return;

    // Overwrite movementInfo
    srcVehicle->m_movementInfo = movementInfo;

    if (srcVehicleGuid != destVehicleGuid)
    {
        Unit* destVehicle = _player->GetMap()->GetUnit(destVehicleGuid);

        if (!destVehicle || !destVehicle->IsVehicle())
            return;

        // Remove Vehicle Control Aura
        srcVehicle->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, _player->GetObjectGuid());

        SpellClickInfoMapBounds clickPair = sObjectMgr.GetSpellClickInfoMapBounds(destVehicle->GetEntry());
        for (SpellClickInfoMap::const_iterator itr = clickPair.first; itr != clickPair.second; ++itr)
            if (itr->second.IsFitToRequirements(_player))
                _player->CastSpell(destVehicle, itr->second.spellId, true);
    }
    else
    {
        srcVehicle->GetVehicleInfo()->Unboard(_player);
        srcVehicle->GetVehicleInfo()->Board(_player, seatId);
    }
}
