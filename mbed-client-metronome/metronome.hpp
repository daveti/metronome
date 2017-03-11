#pragma once

#include "mbed.h"
#include "mbed-client/m2mdevice.h"
#include "mbed-client/m2mobject.h"
#include "mbed-client/m2minterface.h"
#include "mbed-client/m2minterfacefactory.h"

class metronome : public M2MInterfaceObserver
{
public:
    enum { beat_samples = 3 };
    enum {
    	BPM_CURRENT = 0,
    	BPM_MIN = 1,
    	BPM_MAX = 2
	};

public:
    metronome()
    : m_timing(false), m_beat_count(0), m_bpm_current(0), m_bpm_min(0), m_bpm_max(0), m_freq_current(0.0) {}
    ~metronome() {}

public:
	// Call when entering "learn" mode
    void start_timing();
	// Call when leaving "learn" mode
    void stop_timing();

	// Should only record the current time when timing
	// Insert the time at the next free position of m_beats
    void tap();

    bool is_timing() const { return m_timing; }
    
	// Calculate the BPM from the deltas between m_beats
	// Return 0 if there are not enough samples
    size_t get_bpm();
    
    // Set current bpm
    void set_bpm(size_t bpm);
    
    // Reset min/max bpm
    void reset_bpm();
    
    // For frdm_web
    size_t get_bpm_current() const;
    size_t get_bpm_min() const;
    size_t get_bpm_max() const;
    
    // Calculate the frequency
    float get_freq();
    
    // For connector
    void init_freq_dev();
    M2MObject* get_device();       
    void value_updated(M2MBase *base, M2MBase::BaseType type);
    void handle_bpm_reset(void *argument);
    void update_bpm_for_get(int res);
    void update_bpm_for_get_all();
       
    virtual void bootstrap_done(M2MSecurity* security) {}
    virtual void object_registered(M2MSecurity*, const M2MServer&) {}
    virtual void object_unregistered(M2MSecurity*) {}
    virtual void registration_updated(M2MSecurity*, const M2MServer&) {}
    //virtual void value_updated(M2MBase*, M2MBase::BaseType) {}
    virtual void error(M2MInterface::Error error) {}

private:
    bool m_timing;
    Timer m_timer;

	// Insert new samples at the end of the array, removing the oldest
    size_t m_beats[beat_samples];
    size_t m_beat_count;
    float m_beats2[beat_samples];
    volatile size_t m_bpm_current;
    size_t m_bpm_min;
    size_t m_bpm_max;
    float m_freq_current;
    
    // For connector
    M2MObject*  freq_obj;
    Mutex		bpm_mutex;
    //M2MResource* freq_bpm_current_res;
    //M2MResource* freq_bpm_min_res;
    //M2MResource* freq_bpm_max_res;
};
