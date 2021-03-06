/*
 * OLCB_Event_Handler.cpp
 *
 *  Created on: Sep 1, 2011
 *      Author: dgoodman
 *
 *  Much of this code taken from PCE.h and PCE.cpp, written by who, I know not. Bob Jacobsen perhaps?
 */

#include "OLCB_Event_Handler.h"


void OLCB_Event_Handler::update(void) //this method should be overridden to detect conditions for the production of events
{
	if(!isPermitted())
	{
		return;
	}
    // see if any replies are waiting to send
    //Serial.println("update===========");
    while (_sendEvent < _numEvents)
    {
    	//Serial.print("_sendEvent: ");
    	//Serial.println(_sendEvent);
    	//Serial.print("_numEvents: ");
    	//Serial.println(_numEvents);
        // OK to send, see if marked for some cause
        // ToDo: This only sends _either_ producer ID'd or consumer ID'd, not both
        if ( (_events[_sendEvent].flags & (IDENT_FLAG | OLCB_Event::CAN_PRODUCE_FLAG)) == (IDENT_FLAG | OLCB_Event::CAN_PRODUCE_FLAG))
        {
        	//Serial.println("identify produced events!");
            if(_link->sendProducerIdentified(NID, &_events[_sendEvent]))
            {
            	_events[_sendEvent].flags &= ~IDENT_FLAG;    // reset flag
            }
            break; // only send one from this loop
        }
        else if ( (_events[_sendEvent].flags & (IDENT_FLAG | OLCB_Event::CAN_CONSUME_FLAG)) == (IDENT_FLAG | OLCB_Event::CAN_CONSUME_FLAG))
        {
        	  //Serial.println("identify consumed events!");
            if(_link->sendConsumerIdentified(NID, &_events[_sendEvent]))
            {
            	_events[_sendEvent].flags &= ~IDENT_FLAG;    // reset flag
            }
            break; // only send one from this loop
        }
        else if (_events[_sendEvent].flags & PRODUCE_FLAG)
        {
	        //Serial.print("producing ");
	        //Serial.println(_sendEvent, DEC);
            if(_link->sendPCER(NID, &_events[_sendEvent]))
         		  _events[_sendEvent].flags &= ~PRODUCE_FLAG;    // reset flag   
            break; // only send one from this loop
        }
        else if (_events[_sendEvent].flags & TEACH_FLAG)
        {
			//Serial.println("ABOUT TO SEND TEACH MESSAGE");
			//Serial.println(_sendEvent, DEC);
			//Serial.println(_events[_sendEvent].val[6], HEX);
			//Serial.println(_events[_sendEvent].val[7], HEX);
            _link->sendLearnEvent(NID, &_events[_sendEvent]);
            	_events[_sendEvent].flags &= ~TEACH_FLAG;    // reset flag
            break; // only send one from this loop
        }
        else
        {
            // just skip
            ++_sendEvent;
        }
    }
}

void OLCB_Event_Handler::loadEvents(OLCB_Event* events, uint16_t numEvents)
{
    _events = events;
    _numEvents = numEvents;
    _halfEvents = _numEvents>>1;
    //Serial.println("loadevents");
    //Serial.println(_numEvents, DEC);
    //Serial.println(_halfEvents, DEC);
    //Serial.println("loadEvents");
    for(uint16_t i = 0; i < _numEvents; ++i)
    {
    	_events[i].flags = 0;
    	//Serial.print(i, DEC);
    	//Serial.print(" ");
    	//Serial.println(_events[i].flags, HEX);
    }
}

bool OLCB_Event_Handler::produce(uint16_t index) //OLCB_Event *event) //call to produce an event with ID = EID
{
	//Serial.print("produce() ");
	//Serial.println(index);
	bool retval = false;
//	int8_t index = 0;
//	while (-1 != (index = event->findIndexInArray(_events, _numEvents, index)))
//    {
//        // yes, we have to reply with ConsumerIdentified
        if (_events[index].flags & OLCB_Event::CAN_PRODUCE_FLAG)
        {
        	retval = true;
            _events[index].flags |= PRODUCE_FLAG;
            _sendEvent = min(_sendEvent, index);
            //_events[index].print();
        }
//        ++index;
//        if (index>=_numEvents) break;
//    }
    return retval;
}

//this method should be overridden to handle the consumption of events.
//return true if we were able to successfully consume the event, false otherwise
bool OLCB_Event_Handler::consume(uint16_t index)
{
    return false;
}


/* Protocol level interactions for every kind of virtual node */
bool OLCB_Event_Handler::handleMessage(OLCB_Buffer *buffer)
{
    //Serial.println("in OLCB_Event_Handler::handleMessage");
    bool retval = false;
    //first, check to see if it is an event
    if(buffer->isPCEventReport()) //it's what is colloquially known as an event! Or technically as a Producer-Consumer Event Report (!?)
    {
    	  //Serial.println("PCE");
        //great, get the EID, and call consume
        OLCB_Event event;
        buffer->getEventID(&event);
        //check whether the event is in our array
        retval = handlePCEventReport(&event);
    }

    else if(buffer->isIdentifyProducers()) //we may need to respond to this one
    {
    	  //Serial.println("IdentifyProducers");
        // see if we produce the listed event
        OLCB_Event event;
        buffer->getEventID(&event);
        retval = handleIdentifyProducers(&event);
    }

    else if(buffer->isIdentifyConsumers())
    {
    	  //Serial.println("IdentifyConsumers");
        // see if we consume the listed event
        OLCB_Event event;
        buffer->getEventID(&event);
        retval = handleIdentifyConsumers(&event);
    }

    else if(buffer->isIdentifyEvents() || buffer->isIdentifyEventsAddressed())
    {
    	  //Serial.println("IdentifyEvents");
        // See if addressed to us
		if(buffer->isIdentifyEventsAddressed()) //addressed variant
		{
		    //Serial.println("addressed");
		    OLCB_NodeID n;
    		buffer->getDestNID(&n);
   			//Serial.print("got Identify Events for ");
   			//n.print();
   			//NID->print();

			if(n.empty() || *NID == n)//addressed to us
			{
				//Serial.println("to us!");
				retval = handleIdentifyEvents();
			}
		}
		else
		{
			    //Serial.println("global!");
	        retval = handleIdentifyEvents();
	    }
    }

    else if(buffer->isLearnEvent())
    {
        //Serial.println("LearnEvent");
        OLCB_Event event;
        buffer->getEventID(&event);
        retval = handleLearnEvent(&event);
    }

    return retval; //default: not something for us to handle!
}

bool OLCB_Event_Handler::handleIdentifyEvents(void)
{
	for (int i = 0; i < _numEvents; ++i)
	{
		//Serial.print("flagging event ");
		//Serial.println(i, DEC);
		//Serial.println(_events[i].flags, HEX);
		_events[i].flags |= IDENT_FLAG;
	}
	_sendEvent = 0;
  return true;
}

bool OLCB_Event_Handler::handlePCEventReport(OLCB_Event *event)
{
    bool retval = false;
    int16_t index = 0;
    //Serial.println("Received PCER");
	//Serial.println("Can consume?");
    while (-1 != (index = event->findIndexInArray(_events, _numEvents, index)))
    {
        if (_events[index].flags & OLCB_Event::CAN_CONSUME_FLAG)
        {
        	//Serial.println("Yes");
            // yes, notify our own code
            if(consume(index))
            {
                retval = true;
            }
        }
        ++index;
        if (index>=_numEvents)
        {
            break;
        }
    }
//    if(!retval)
//	    //Serial.println("No");
    return retval;
}

bool OLCB_Event_Handler::handleIdentifyProducers(OLCB_Event *event)
{
    bool retval = false;
    int16_t index = 0;
    // find producers of event
    while (-1 != (index = event->findIndexInArray(_events, _numEvents, index)))
    {
        // yes, we have to reply with ProducerIdentified
        if (_events[index].flags & OLCB_Event::CAN_PRODUCE_FLAG)
        {
            retval = true;
            _events[index].flags |= IDENT_FLAG;
            _sendEvent = min(_sendEvent, index);
        }
        ++index;
        if (index>=_numEvents) break;
    }
    // ToDo: add identify flags so that events that are both produced and consumed
    // have only one form sent in response to a specific request.
    return retval;
}

bool OLCB_Event_Handler::handleIdentifyConsumers(OLCB_Event *event)
{
    bool retval = false;
    int index = 0;
    // find consumers of event
    //event->print();
    while (-1 != (index = event->findIndexInArray(_events, _numEvents, index)))
    {
    	//Serial.print("found event at ");
    	//Serial.println(index, DEC);
        // yes, we have to reply with ConsumerIdentified
        if (_events[index].flags & OLCB_Event::CAN_CONSUME_FLAG)
        {
            retval = true;
            _events[index].flags |= IDENT_FLAG;
            _sendEvent = min(_sendEvent, index);
        }
        ++index;
        if (index>=_numEvents) break;
    }
    //Serial.println(index, DEC);
    return retval;
}

bool OLCB_Event_Handler::handleLearnEvent(OLCB_Event *event)
{
	//Serial.println(event->val[7]);
	
    bool save = false;
    for (uint16_t i=0; i<_numEvents; ++i)
    {
        if ( (_events[i].flags & LEARN_FLAG ) != 0 )
        {
            memcpy(_events[i].val, event->val, 8);
            _events[i].flags |= IDENT_FLAG; // notify new eventID
            _events[i].flags &= ~LEARN_FLAG; // enough learning
            _sendEvent = min(_sendEvent, i);
            save = true;
        }
    }

    if (save)
    {
        store(); //TODO check for success here!
    }
    return save;
}

void OLCB_Event_Handler::newEvent(int index, bool p, bool c)
{

  _events[index].flags |= IDENT_FLAG;
  _sendEvent = min(_sendEvent, index);
  if (p) _events[index].flags |= OLCB_Event::CAN_PRODUCE_FLAG;
  if (c) _events[index].flags |= OLCB_Event::CAN_CONSUME_FLAG;


  //Serial.print("new event ");
      	//Serial.print(index, DEC);
    	//Serial.print(" ");
    	//Serial.println(_events[index].flags, HEX);
}

void OLCB_Event_Handler::markToLearn(int index, bool mark)
{
    if (mark)
        _events[index].flags |= LEARN_FLAG;
    else
        _events[index].flags &= ~LEARN_FLAG;
}

void OLCB_Event_Handler::markToTeach(int index, bool mark)
{
    if (mark)
        _events[index].flags |= TEACH_FLAG;
    else
        _events[index].flags &= ~TEACH_FLAG;
    _sendEvent = min(_sendEvent, index);
}

bool OLCB_Event_Handler::markedToLearn(int index)
{
	return(_events[index].flags & LEARN_FLAG);
}

bool OLCB_Event_Handler::markedToTeach(int index)
{
	return(_events[index].flags & TEACH_FLAG);
}

bool OLCB_Event_Handler::store(void)
{
    return false; //false for not saved!
}

bool OLCB_Event_Handler::load(void)
{
    return false;
}
