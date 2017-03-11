#include "mbed.h"
#include "utils.hpp"

#include "EthernetInterface.h"
#include "frdm_client.hpp"

#include "metronome.hpp"

#define IOT_ENABLED

namespace active_low
{
	const bool on = false;
	const bool off = true;
}

DigitalOut g_led_red(LED1);
DigitalOut g_led_green(LED2);
DigitalOut g_led_blue(LED3);

InterruptIn g_button_mode(SW3);
InterruptIn g_button_tap(SW2);
			

metronome *g_met;
bool g_play;
bool g_learn;
bool g_bpm;
bool g_debug = false;
bool g_debug2 = false;
Mutex g_mutex;
Semaphore g_sam(0);

void dark()
{
	g_led_red = active_low::off;
	g_led_green = active_low::off;
	g_led_blue = active_low::off;
}

void bright()
{
	g_led_red = active_low::on;
	g_led_green = active_low::on;
	g_led_blue = active_low::on;
}

void on_mode()
{
    //dark();    
    
    // Change modes
    //dark();
    if (g_met->is_timing()) {
    	// Play mode
		//g_met->stop_timing();
		g_play = true;
		g_learn = false;
		g_bpm = true;
		//if (g_debug2)
			//utils::pulse(g_led_green);
    } else {
    	// Learn mode
    	g_met->start_timing();
    	g_play = false;
    	//dark();
    }
    //g_sam.release();
}

void on_tap()
{
    // Receive a tempo tap
    if ((!g_met->is_timing()) || (g_play))
    	return;
    	
    // Simplify the ISR
    g_learn = true;
    g_sam.release();
    	
    //dark();
   
   	// Save the sample
   	// Q: shall we consider the delay caused by pulse? 	
    //g_met->tap();
    
    // Q: can we do pulse here?
	//utils::pulse(g_led_red);
}

int main()
{
	// Seed the RNG for networking purposes
    unsigned seed = utils::entropy_seed();
    srand(seed);
    
    // Init met
    g_met = new metronome();
#ifdef IOT_ENABLED
    // Init freq dev
    g_met->init_freq_dev();
#endif

	// LEDs are active LOW - true/1 means off, false/0 means on
	// Use the constants for easier reading
    //g_led_red = active_low::on;
    //g_led_green = active_low::on;
    //g_led_blue = active_low::on;
	g_led_red = active_low::off;
	g_led_green = active_low::off;
	g_led_blue = active_low::off;

	// Button falling edge is on push (rising is on release)
    g_button_mode.fall(&on_mode);
    g_button_tap.fall(&on_tap);

#ifdef IOT_ENABLED
	// Turn on the blue LED until connected to the network
    g_led_blue = active_low::on;

	// Need to be connected with Ethernet cable for success
    EthernetInterface ethernet;
    if (ethernet.connect() != 0)
        return 1;

	// Pair with the device connector
    frdm_client client("coap://api.connector.mbed.com:5684", &ethernet);
    if (client.get_state() == frdm_client::state::error)
        return 1;

	// The REST endpoints for this device
	// Add your own M2MObjects to this list with push_back before client.connect()
    M2MObjectList objects;

    M2MDevice* device = frdm_client::make_device();
    objects.push_back(device);
    
    // Add the freq dev
    objects.push_back(g_met->get_device());

	// Publish the RESTful endpoints
    client.connect(objects);

	// Connect complete; turn off blue LED forever
    g_led_blue = active_low::off;
#endif

    while (true)
    {
#ifdef IOT_ENABLED
        if (client.get_state() == frdm_client::state::error)
            break;
#endif

		if (g_debug2) {
			utils::pulse(g_led_red);
			wait(1);
		}
		
		g_sam.wait(100);

        // Insert any code that must be run continuously here
        //g_mutex.lock();
        if (g_play) {
        	// Stop timing
        	g_met->stop_timing();
        	
        	// Compute the BPM here
        	if (g_bpm) {
        		g_bpm = false;
        		g_met->get_bpm();
#ifdef IOT_ENABLED
				g_met->update_bpm_for_get_all();
#endif
			}
        	
        	//utils::pulse(g_led_green, g_met->get_freq());
        	//utils::pulse(g_led_green, 1);
        	//debug
        	if (g_debug2) {
				utils::pulse(g_led_blue);
        	}
        	if (g_debug) {
        		if (g_met->get_freq() > 1) {
        			g_led_blue = true;
        			wait(0.5);
        			g_led_blue = false;
        			wait(0.5);
        			continue;
        		}
        	}
        	
        	//g_led_green = true;
        	//wait(g_met->get_freq()/2);
        	//wait(0.5);
        	//g_led_green = false;
        	//wait(g_met->get_freq()/2);
        	//wait(0.5);
        	utils::pulse(g_led_green, g_met->get_freq());
        } else if (g_learn) {
        	 g_learn = false;
        	 g_met->tap();
        	 utils::pulse(g_led_red);
        } else {
        		dark();
        		wait(0.2);
        }
        //g_mutex.unlock();
    }
    
    if (g_debug2)
    	bright();

#ifdef IOT_ENABLED
    client.disconnect();
#endif

    return 1;
}
