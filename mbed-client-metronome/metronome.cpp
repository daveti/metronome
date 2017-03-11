#include "metronome.hpp"

// Call when entering "learn" mode
void metronome::start_timing()
{
    if (this->m_timing)
        return;
        
    this->m_timing = true;
    this->m_beat_count = 0;
    this->m_bpm_current = 0;
    this->m_bpm_min = 0;
    this->m_bpm_max = 0;
    this->m_freq_current = 0.0;
    this->m_timer.start();
}

// Call when leaving "learn" mode
void metronome::stop_timing()
{
    if (this->m_timing == false)
        return;
        
    this->m_timing = false;
    this->m_timer.stop();
}

// Should only record the current time when timing
// Insert the time at the next free position of m_beats
void metronome::tap()
{
    this->m_timer.stop();
    this->m_beats2[this->m_beat_count % beat_samples] = this->m_timer.read();
    this->m_beat_count++;
    this->m_timer.reset();
    this->m_timer.start();
}

// Calculate the BPM from the deltas between m_beats
// Return 0 if there are not enough samples
size_t metronome::get_bpm()
{
    float avg, min, max;
    int i;
    
    if (this->m_beat_count < beat_samples)
        return 0;
    
    if (this->m_bpm_current)
        return this->m_bpm_current;
        
    avg = 0;
    min = 7777777;
    max = 0;
    for (i = 0; i < beat_samples; i++) {
        avg += this->m_beats2[i];
        if (min > this->m_beats2[i])
            min = this->m_beats2[i];
        if (max < this->m_beats2[i])
            max = this->m_beats2[i];
    }

    bpm_mutex.lock();    
    this->m_bpm_current = size_t(60/(avg/3));
    this->m_bpm_min = size_t(60/max);
    this->m_bpm_max = size_t(60/min);
    bpm_mutex.unlock();
    
    return this->m_bpm_current;
}

void metronome::set_bpm(size_t bpm)
{
    this->m_bpm_current = bpm;
}

void metronome::reset_bpm(void)
{
    bpm_mutex.lock();
    this->m_bpm_min = 0;
    this->m_bpm_max = 0;
    bpm_mutex.unlock();
    
    // Update the resource
    this->update_bpm_for_get(BPM_MIN);
    this->update_bpm_for_get(BPM_MAX);
}
    
// Calculate the frequency
float metronome::get_freq()
{
    int i;
    float avg;

    // Handle the PUT    
    if (this->m_bpm_current != 0) {
        avg = float(this->m_bpm_current)/60;
        this->m_freq_current = 1/avg;
        return this->m_freq_current;
    }
    
    // Handle dup if no PUT      
    if (this->m_freq_current != 0)
        return this->m_freq_current;

    // Handle play mode        
    if (this->m_beat_count < beat_samples)
        return 777777;

    avg = 0;    
    for (i = 0; i < beat_samples; i++)
        avg += this->m_beats2[i];
    
    this->m_freq_current = avg/3;
    
    return this->m_freq_current;
}

// For frdm_web
size_t metronome::get_bpm_current() const
{
    return this->m_bpm_current;
}

size_t metronome::get_bpm_min() const
{
    return this->m_bpm_min;
}

size_t metronome::get_bpm_max() const
{
    return this->m_bpm_max;
}



// For connector
void metronome::init_freq_dev()
{
    // Create the freq device
    this->freq_obj = M2MInterfaceFactory::create_object("3318");
    M2MObjectInstance* freq_inst = this->freq_obj->create_object_instance();
            
    // 5900 = get/set current bpm
    M2MResource* bpm_current_res = freq_inst->create_dynamic_resource("5900", "bpm_current",
                                                M2MResourceInstance::INTEGER, true);
    // read and write
    bpm_current_res->set_operation(M2MBase::GET_PUT_ALLOWED);
    // set initial value
    bpm_current_res->set_value((uint8_t*)"0", 1);
    // set callback
    //bpm_current_res->set_value_updated_function(value_updated_callback(this, &metronome::handle_bpm_current_put));
    //this->freq_bpm_current_res = bpm_current_res;

            
    // 5601 = get min bpm
    M2MResource* bpm_min_res = freq_inst->create_dynamic_resource("5601", "bpm_min",
                                                M2MResourceInstance::INTEGER, true);
    // read
    bpm_min_res->set_operation(M2MBase::GET_ALLOWED);
    // set initial value
    bpm_min_res->set_value((uint8_t*)"0", 1);
    //this->freq_bpm_min_res = bpm_min_res;
            
            
    // 5602 = get max bpm
    M2MResource* bpm_max_res = freq_inst->create_dynamic_resource("5602", "bpm_max",
                                                M2MResourceInstance::INTEGER, true);
    // read
    bpm_max_res->set_operation(M2MBase::GET_ALLOWED);
    // set initial value
    bpm_max_res->set_value((uint8_t*)"0", 1);
    //this->freq_bpm_max_res = bpm_max_res;
            
            
    // 5605 = reset min/max bpm
    M2MResource* bpm_reset_res = freq_inst->create_dynamic_resource("5605", "bpm_reset",
                                                M2MResourceInstance::INTEGER, true);
    // write
    bpm_reset_res->set_operation(M2MBase::POST_ALLOWED);
    // set initial value
    bpm_reset_res->set_value((uint8_t*)"0", 1);
    // set callback
    bpm_reset_res->set_execute_function(execute_callback(this, &metronome::handle_bpm_reset));
            
            
    // 5701 = get "bpm"
    M2MResource* bpm_unit_res = freq_inst->create_dynamic_resource("5701", "bpm_unit",
                                                M2MResourceInstance::INTEGER, true);
    // read
    bpm_unit_res->set_operation(M2MBase::GET_ALLOWED);
    // set initial value
    bpm_unit_res->set_value((uint8_t*)"bpm", 3);
}

M2MObject* metronome::get_device()
{
    return this->freq_obj;
}
  
// Overide of the original value_update      
void metronome::value_updated(M2MBase *base, M2MBase::BaseType type)
{
    M2MResource* resource = NULL;
    M2MResourceInstance* res_instance = NULL;
    M2MObjectInstance* obj_instance = NULL;
    M2MObject* obj = NULL;
    String object_name = "";
    String resource_name = "";
    uint16_t object_instance_id = 0;
    uint16_t resource_instance_id = 0;
    if(base) {
        switch(base->base_type()) {
        case M2MBase::Object:
            obj = (M2MObject *)base;
            object_name = obj->name();
            break;
        case M2MBase::ObjectInstance:
            obj_instance = (M2MObjectInstance *)base;
            object_name = obj_instance->name();
            object_instance_id = obj_instance->instance_id();
            break;
        case M2MBase::Resource: {
            resource = (M2MResource*)base;
            object_name = resource->object_name();
            object_instance_id = resource->object_instance_id();
            resource_name = resource->name();
            //printf("Value updated, object name %s, object instance id %d, resource name %s\r\n",
                           //resource->object_name().c_str(), resource->object_instance_id(), resource->name().c_str());
            //Q: defensive checking?
            String value = resource->get_value_string();
            int new_value = atoi(value.c_str());
            bpm_mutex.lock();
            this->m_bpm_current = new_value;
            bpm_mutex.unlock();
            this->update_bpm_for_get(BPM_CURRENT);
            }
            break;
        case M2MBase::ResourceInstance: {
            res_instance = (M2MResourceInstance*)base;
            object_name = res_instance->object_name();
            object_instance_id = res_instance->object_instance_id();
            resource_name = res_instance->name();
            resource_instance_id = res_instance->instance_id();
            //Q: defensive checking?
            String value = res_instance->get_value_string();
            int new_value = atoi(value.c_str());
            bpm_mutex.lock();
            this->m_bpm_current = new_value;
            bpm_mutex.unlock();
            this->update_bpm_for_get(BPM_CURRENT);
            }
            break;
        default:
            break;
        }
    }
}
        
void metronome::handle_bpm_reset(void *argument)
{
    if(argument) {
        M2MResource::M2MExecuteParameter* param = (M2MResource::M2MExecuteParameter*)argument;
        int payload_length = param->get_argument_value_length();
        uint8_t* payload = param->get_argument_value();
        String object_name = param->get_argument_object_name();
        uint16_t object_instance_id = param->get_argument_object_instance_id();
        String resource_name = param->get_argument_resource_name();
        // Q: shall we do defensive checking inside before resetting?
    }
            
    // Reset the bpm regardless
    this->reset_bpm();
}

void metronome::update_bpm_for_get(int res)
{
    char buf[16] = {0};
    M2MResource* res2;
    
    // Get the damn obj
    M2MObjectInstance* inst = this->freq_obj->object_instance();
        
    switch (res) {
    case BPM_CURRENT:
        snprintf(buf, 16, "%d", this->m_bpm_current);
        res2 = inst->resource("5900");
        res2->set_value((uint8_t*)buf, strlen(buf));
        //this->freq_bpm_current_res->set_value((uint8_t*)buf, strlen(buf));
        break;
    
    case BPM_MIN:
        snprintf(buf, 16, "%d", this->m_bpm_min);
        res2 = inst->resource("5601");
        res2->set_value((uint8_t*)buf, strlen(buf));
        //this->freq_bpm_min_res->set_value((uint8_t*)buf, strlen(buf));
        break;
        
    case BPM_MAX:
        snprintf(buf, 16, "%d", this->m_bpm_max);
        res2 = inst->resource("5602");
        res2->set_value((uint8_t*)buf, strlen(buf));
        //this->freq_bpm_max_res->set_value((uint8_t*)buf, strlen(buf));
        break;
        
    default:
        break;
    }
}

void metronome::update_bpm_for_get_all()
{
    this->update_bpm_for_get(BPM_CURRENT);
    this->update_bpm_for_get(BPM_MIN);
    this->update_bpm_for_get(BPM_MAX);
}


