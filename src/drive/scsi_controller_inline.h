/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef SCSI_CONTROLLER_INLINE_H
#define SCSI_CONTROLLER_INLINE_H

#include "scsi_controller.h"

INLINE bool ScsiController::IsFitted() const
{
    return m_fitted;
}

INLINE ScsiDisk* ScsiController::GetDisk(int id)
{
    return (id >= 0 && id < SCSI_TARGETS) ? &m_disks[id] : NULL;
}

INLINE ScsiController::ScsiController_State* ScsiController::GetState()
{
    return &m_state;
}

INLINE u8 ScsiController::GetBusOutputs(u8 control) const
{
    if ((control & k_scsi_control_wen) == 0)
        return 0;

    return control & (k_scsi_control_atn | k_scsi_control_sel | k_scsi_control_rst);
}

INLINE u8 ScsiController::ReadStatus() const
{
    return m_state.signals | (m_state.interrupt ? k_scsi_status_int : 0);
}

INLINE bool ScsiController::IsInputPhase() const
{
    return (m_state.phase == SCSI_PHASE_DATA_IN) || (m_state.phase == SCSI_PHASE_STATUS) ||
        (m_state.phase == SCSI_PHASE_MESSAGE_IN);
}

INLINE bool ScsiController::IsOutputPhase() const
{
    return (m_state.phase == SCSI_PHASE_COMMAND) || (m_state.phase == SCSI_PHASE_DATA_OUT) ||
        (m_state.phase == SCSI_PHASE_MESSAGE_OUT);
}

#endif /* SCSI_CONTROLLER_INLINE_H */
