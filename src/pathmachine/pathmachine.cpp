/************************************************************************
**
** Authors:   Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve), 
**            Marek Krejza <krejza@gmail.com> (Caligor)
**
** This file is part of the MMapper2 project. 
** Maintained by Marek Krejza <krejza@gmail.com>
**
** Copyright: See COPYING file that comes with this distribution
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file COPYING included in the packaging of
** this file.  
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
** LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
** OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
** WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
*************************************************************************/

#include <stack>

#include "pathmachine.h"

using namespace std;
using namespace Qt;

PathMachine::PathMachine(AbstractRoomFactory * in_factory, bool threaded) :
    Component(threaded),
    factory(in_factory),
    signaler(this),
    pathRoot(0,0,0),
    mostLikelyRoom(0,0,0),
    lastEvent(0),
    state(SYNCING),
    paths(new list<Path *>)
{}

void PathMachine::init()
{
  QObject::connect(&signaler, SIGNAL(scheduleAction(MapAction *)), this, SIGNAL(scheduleAction(MapAction *)));
}

/**
 * the signals - apart from playerMoved have to be DirectConnections because we want to 
 * be sure to find all available rooms and we also want to make sure a room is actually
 * inserted into the map before we search for it
 * The slots need to be queued because we want to make sure all data is only accessed
 * from this thread
 */
ConnectionType PathMachine::requiredConnectionType(const QString & str)
{

  if (str == SLOT(event(ParseEvent *)) ||
      str == SLOT(deleteMostLikelyRoom()))
    return QueuedConnection;
  else if (str == SIGNAL(playerMoved(Coordinate, Coordinate)))
    return AutoCompatConnection;
  else
    return DirectConnection;
}

void PathMachine::releaseAllPaths()
{
  for (list<Path*>::iterator i = paths->begin(); i != paths->end(); ++i)
    (*i)->deny();
  paths->clear();

  state = SYNCING;
}

void PathMachine::retry()
{

  switch (state)
  {
  case APPROVED:
    state = SYNCING;
    break;
  case EXPERIMENTING:
    set<Path *> prevPaths;
    list<Path *> * newPaths = new list<Path *>;
    for (list<Path*>::iterator i = paths->begin(); i != paths->end(); ++i)
    {
      Path * working = *i;
      Path * previous = working->getParent();
      previous->removeChild(working);
      working->setParent(0);
      working->deny();
      if (previous && (prevPaths.find(previous) == prevPaths.end()))
      {
        newPaths->push_back(previous);
        prevPaths.insert(previous);
      }
    }
    delete paths;
    paths = newPaths;
    if (paths->empty()) state = SYNCING;
    break;
  }
  if (lastEvent) event(lastEvent);
}

void PathMachine::event(ParseEvent * ev)
{
  if (ev != lastEvent)
  {
    delete lastEvent;
    lastEvent = ev;
  }
  switch (state)
  {
  case APPROVED:
    approved(ev);
    break;
  case EXPERIMENTING:
    experimenting(ev);
    break;
  case SYNCING:
    syncing(ev);
    break;
  }
}

void PathMachine::deleteMostLikelyRoom()
{
  if (state == EXPERIMENTING)
  {
    list<Path *> * newPaths = new list<Path *>;
    Path * best = 0;
    for (list<Path*>::iterator i = paths->begin(); i != paths->end(); ++i)
    {
      Path * working = *i;
      if ((working)->getRoom()->getId() == mostLikelyRoom.getId()) working->deny();
      else if (best == 0) best = working;
      else if (working->getProb() > best->getProb())
      {
        newPaths->push_back(best);
        best = working;
      }
      else newPaths->push_back(working);
    }
    if (best) newPaths->push_front(best);
    delete paths;
    paths = newPaths;

  }
  else
  {
    paths->clear(); // throw the parser into syncing
  }
  evaluatePaths();
}



void PathMachine::tryExits(const Room * room, RoomRecipient * recipient, ParseEvent * event, bool out)
{
  uint move = event->getMoveType();
  if (move < (uint)room->getExitsList().size())
  {
    const Exit & possible = room->exit(move);
    tryExit(possible, recipient, out);
  }
  else
  {
    emit lookingForRooms(recipient, room->getId());
    if (move >= factory->numKnownDirs())
    {
      const ExitsList & eList = room->getExitsList();
      for (int listit = 0; listit < eList.size(); ++listit)
      {
        const Exit & possible = eList[listit];
        tryExit(possible, recipient, out);
      }
    }
  }
}

void PathMachine::tryExit(const Exit & possible, RoomRecipient * recipient, bool out)
{
  set<uint>::const_iterator begin;
  set<uint>::const_iterator end;
  if (out)
  {
    begin = possible.outBegin();
    end = possible.outEnd();
  }
  else
  {
    begin = possible.inBegin();
    end = possible.inEnd();
  }
  for (set<uint>::const_iterator i = begin; i != end; ++i)
  {
    emit lookingForRooms(recipient, *i);
  }
}


void PathMachine::tryCoordinate(const Room * room, RoomRecipient * recipient, ParseEvent * event)
{
  uint moveCode = event->getMoveType();
  uint size = factory->numKnownDirs();
  if (size > moveCode)
  {
    Coordinate c = room->getPosition() + factory->exitDir(moveCode);
    emit lookingForRooms(recipient, c);
  }
  else
  {
    Coordinate roomPos = room->getPosition();
    for (uint i = 0; i < size; ++i)
    {
      emit lookingForRooms(recipient, roomPos + factory->exitDir(i));
    }
  }
}

void PathMachine::approved(ParseEvent * event)
{
  Approved appr(factory, event, params.matchingTolerance);
  const Room * perhaps = 0;

  tryExits(&mostLikelyRoom, &appr, event, true);

  perhaps = appr.oneMatch();

  if (!perhaps)
  { // try to match by reverse exit
    appr.reset();
    tryExits(&mostLikelyRoom, &appr, event, false);
    perhaps = appr.oneMatch();
    if (!perhaps)
    { // try to match by coordinate
      appr.reset();
      tryCoordinate(&mostLikelyRoom, &appr, event);
      perhaps = appr.oneMatch();
      if (!perhaps)
      { // try to match by coordinate one step below expected
        const Coordinate & eDir = factory->exitDir(event->getMoveType());
        if (eDir.z == 0)
        {
          appr.reset();
          Coordinate c = mostLikelyRoom.getPosition() + eDir;
          c.z--;
          emit lookingForRooms(&appr, c);
          perhaps = appr.oneMatch();

          if (!perhaps)
          { // try to match by coordinate one step above expected
            appr.reset();
            c.z += 2;
            emit lookingForRooms(&appr, c);
            perhaps = appr.oneMatch();
          }
        }
      }
    }
  }
  if (perhaps)
  {
    uint move = event->getMoveType();
    if ((uint)mostLikelyRoom.getExitsList().size() > move)
    {
      emit scheduleAction(new AddExit(mostLikelyRoom.getId(), perhaps->getId(), move));
    }
    mostLikelyRoom = *perhaps;
    emit playerMoved(mostLikelyRoom.getPosition());
  }
  else
  { // couldn't match, give up
    state = EXPERIMENTING;
    pathRoot = mostLikelyRoom;
    Path * root = new Path(&pathRoot, 0, 0, &signaler);
    paths->push_front(root);
    experimenting(event);
  }
}


void PathMachine::syncing(ParseEvent * event)
{
  Syncing sync(params, paths, &signaler);
  if (event->getNumSkipped() <= params.maxSkipped) 
    emit lookingForRooms(&sync, event);
  paths = sync.evaluate();
  evaluatePaths();
}


void PathMachine::experimenting(ParseEvent * event)
{
  Experimenting * exp = 0;
  uint moveCode = event->getMoveType();
  Coordinate move = factory->exitDir(moveCode);
  // only create rooms if no properties are skipped and
  // the move coordinate is not 0,0,0
  if (event->getNumSkipped() == 0 && (moveCode < (uint)mostLikelyRoom.getExitsList().size()))
  {
    uint dirCode = event->getMoveType();
    exp = new Crossover(paths, dirCode, params, factory);
    set<const Room *> pathEnds;
    for (list<Path *>::iterator i = paths->begin(); i != paths->end(); ++i)
    {
      const Room * working = (*i)->getRoom();
      if (pathEnds.find(working) == pathEnds.end())
      {
        emit createRoom(event, working->getPosition() + move);
        pathEnds.insert(working);
      }
    }
    emit lookingForRooms(exp, event);
  }
  else
  {
    OneByOne * oneByOne = new OneByOne(factory, event, params, &signaler);
    exp = oneByOne;
    for (list<Path *>::iterator i = paths->begin(); i != paths->end(); ++i)
    {
      const Room * working = (*i)->getRoom();
      oneByOne->addPath(*i);
      tryExits(working, exp, event, true);
      tryExits(working, exp, event, false);
      tryCoordinate(working, exp, event);
    }
  }

  paths = exp->evaluate();
  delete exp;
  evaluatePaths();
}

void PathMachine::evaluatePaths()
{
  if (paths->empty())
  {
    state = SYNCING;
  }
  else
  {
    mostLikelyRoom = *(paths->front()->getRoom());
    if (++paths->begin() == paths->end())
    {
      state = APPROVED;
      paths->front()->approve();
      paths->pop_front();
    }
    else
    {
      state = EXPERIMENTING;
    }
    emit playerMoved(mostLikelyRoom.getPosition());
  }
}





