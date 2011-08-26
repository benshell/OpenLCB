/***************************************************************************************
CommandStationX2011
A demonstration of a very basic OpenLCB DCC command station.
Copyright (C)2011 D.E. Goodman-Wilson

This file is part of ThrottleX2011.

    CommandStationX2011 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CommandStationX2011.  If not, see <http://www.gnu.org/licenses/>.
    
***************************************************************************************/

#include "LocomotiveFactory.h"
#include "OLCB_CAN_Link.h"
//#include <MemoryFree.h>

//This is where we watch to see if a new request for a locomotive is likely to occur, and create an
//instance of the Locomotive class to service it. We check by watching for verifyNID messages, which
//will be generated by nodes that need to discover the alias of a given NID. Since locomotive NIDs
//are well-known, we can take advantage of this mechanism to instantiate Locomotive instances
//just-in-time.
//Notice that this method will only ever be called if no other existing Locomotive instance
//has already handled it.
bool LocomotiveFactory::verifyNID(OLCB_NodeID *nid)
{
//  Serial.println("Verify request made it to LocomotiveFactory!");
//  Serial.println(freeMemory(),DEC);

  if( (nid->val[0] == 6) && (nid->val[1] == 1) ) //if it's intended for a DCC locomotive, we ought to pay attention!
  {
    //find a slot for it
//    Serial.println("LocoFactory: Got a request to create a new loco vnode");
    for(int i = 0; i < NUM_SLOTS; ++i)
    {
      if(_locos[i].isAvailable()) //an empty slot is found!
      {
//        Serial.print("    Installing in slot ");
//        Serial.println(i,DEC);
//        Serial.println(freeMemory(),DEC);
        _locos[i].setLink(_link);
//        Serial.println("Set the link");
//        Serial.println(freeMemory(),DEC);
        _locos[i].setNID(nid);
//        Serial.println("set the NID");
//        Serial.println(freeMemory(),DEC);
        _locos[i].verified = false; //just in case
//        Serial.println(freeMemory(),DEC);
        //Serial.println(freeMemory(),DEC);
//        Serial.println("Done installing loco");
        return false; //what the what? we're actually not yet ready to send out the verifiedNID packet, as we don't yet have an alias.
        //That's up to the virtual node to do on its own!
      }
    }
//    Serial.println("    Out of slots. Too bad."); //TODO Need to figure out what to do in this case?
    //It won't do to let the throttle requesting the loco just hang, we need to tell it something informative.
    //Problem is, without enough memory, we can't request an alias to go with the requested NID, and so
    //we can't respond /as/ the loco being queried. Need a mechanism for telling a throttle about this?
    //We could use the LocoFactory's alias, then pass it a datagram to inform it of why it's request
    //is invalid, and then send out a message indicating that the alias is being invalidated for that NID. Need
    //a method for invalidating aliases.
  }
//  Serial.println("No room!");
//  Serial.println(freeMemory(),DEC);
  return false; //no room availble, or not a request for a loco. (see above)
}

void LocomotiveFactory::update(void)
{
  //This is where we should force a verifiedID from any recently created loco, as it would have missed
  // the initial request.
  for(int i = 0; i < NUM_SLOTS; ++i)
  {
    if(!_locos[i].verified && _locos[i].NID->alias) //"verified" is just a flag to let us know that it hasn't verified its
    // NID yet; checking alias ensures that it has been assigned one. In this case, we need to tell the node to
    // send out a verified ID message.
    {
      //Serial.println("Sending VerifedNID");
      ((OLCB_CAN_Link*)_link)->sendVerifiedNID(_locos[i].NID);
      _locos[i].verified = true;
    }
  }
}

/* bool LocomotiveFactory::processDatagram(void)
{
  //The only datagrams we care about are attach requests. We will have received them only if an existing loco hasn't handled it. In which case, we need to create the loco,
  //and assign it the NID from the attach request.
//  Serial.println("LocomotiveFactory got a datagram!");
  if( (_rxDatagramBuffer->data[0] == DATAGRAM_MOTIVE) && (_rxDatagramBuffer->data[0] == DATAGRAM_MOTIVE_ATTACH))
  {
//    Serial.println("LocoFactory: Got a request to create a new loco vnode (via ATTACH request)");
    for(int i = 0; i < NUM_SLOTS; ++i) //there has to be a mre efficient way to store this.
    {
      OLCB_NodeID nid;
      //Now figure out the NID. The datagrambuffer should have it all.
      nid.copy(&(_rxDatagramBuffer->destination));
      if(_locos[i].isAvailable()) //an empty slot is found!
      {
//        Serial.print("    Installing in slot ");
//        Serial.println(i,DEC);
        _locos[i].setLink(_link);
        _locos[i].setNID(&nid);
        _locos[i].verified = true; //because it should be
        //this is a dirty hack.
        memcpy(&(_locos[i]._rxDatagramBuffer), &_rxDatagramBuffer, sizeof(OLCB_Datagram));
        _locos[i].attachDatagram();
        return true; //we are ACKing with the wrong source NID. Oh well. This should be fixed later, but should be OK for now? I hope? TODO
      }
    }
//    Serial.println("    Out of slots. Too bad."); //TODO Need to figure out what to do in this case?
    //It won't do to let the throttle requesting the loco just hang, we need to tell it something informative.
    //Problem is, without enough memory, we can't request an alias to go with the requested NID, and so
    //we can't respond /as/ the loco being queried. Need a mechanism for telling a throttle about this?
    //We could use the LocoFactory's alias, then pass it a datagram to inform it of why it's request
    //is invalid, and then send out a message indicating that the alias is being invalidated for that NID. Need
    //a method for invalidating aliases.
  }
  return false;
}*/


//TODO NOT ALL REQUESTS COME THROUGH VERIFY IDs!!!! Sometimes they come through plain old Attach requests! This happens when a loco has been released and the is re-attached.
