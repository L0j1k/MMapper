/************************************************************************
**
** Authors:   Nils Schimmelmann <nschimme@gmail.com>
**
** This file is part of the MMapper project.
** Maintained by Nils Schimmelmann <nschimme@gmail.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the:
** Free Software Foundation, Inc.
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************************************************************************/

#include "CommandQueue.h"

#include "../mapdata/ExitDirection.h"

QByteArray CommandQueue::toByteArray() const
{
    QByteArray dirs;
    for (int i = 0; i < base::size(); i++) {
        const auto cmd = base::at(i);
        // REVISIT: Serialize/deserialize directions more intelligently
        dirs.append(Mmapper2Exit::charForDir(static_cast<ExitDirection>(cmd)));
    }
    return dirs;
}

CommandQueue &CommandQueue::operator=(const QByteArray &dirs)
{
    base::clear();
    for (int i = 0; i < dirs.length(); i++) {
        base::enqueue(static_cast<CommandIdType>(Mmapper2Exit::dirForChar(dirs.at(i))));
    }
    return *this;
}