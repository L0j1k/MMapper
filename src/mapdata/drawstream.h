#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../display/MapCanvasRoomDrawer.h" // LayerToRooms
#include "../mapfrontend/AbstractRoomVisitor.h"

class Room;

class DrawStream final : public AbstractRoomVisitor
{
public:
    explicit DrawStream(LayerToRooms &layerToRooms);
    virtual ~DrawStream() override;
    virtual void visit(const Room *room) override;

private:
    LayerToRooms &layerToRooms;
};
