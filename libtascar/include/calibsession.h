#ifndef CALIBSESSION_H
#define CALIBSESSION_H

#include "jackiowav.h"
#include "session.h"

namespace TASCAR {

  /**
     @brief Parameters for loudspeaker equalization and calibration measurement.

     This class stores configuration parameters used during the calibration
     process. It distinguishes between parameters for broadband speakers and
     subwoofers. Parameters include frequency ranges, stimulus duration,
     reference levels, and filterbank settings for frequency analysis.
   */
  class spk_eq_param_t {
  public:
    /**
       @brief Constructor
       @param issub If true, initializes parameters for subwoofer calibration;
                   otherwise initializes for broadband speakers.
     */
    spk_eq_param_t(bool issub = false);

    /**
       @brief Resets all parameters to their factory default values.
       Defaults depend on whether this instance is configured for a subwoofer
       or a broadband speaker.
     */
    void factory_reset();

    /**
       @brief Reads parameters from the global TASCAR configuration.
       This loads default values defined in the system configuration files.
     */
    void read_defaults();

    /**
       @brief Writes current parameters to the global TASCAR configuration.
       This saves the current settings as the new defaults.
     */
    void write_defaults();

    /**
       @brief Reads parameters from an XML layout node.
       @param layoutnode The XML node containing speaker configuration.
     */
    void read_xml(const tsccfg::node_t& layoutnode);

    /**
       @brief Saves current parameters to an XML layout node.
       @param layoutnode The XML node to write configuration to.
     */
    void save_xml(const tsccfg::node_t& layoutnode) const;

    /**
       @brief Validates the current parameter set.
       @throws TASCAR::ErrMsg if any parameter is out of its valid range
               (e.g., fmin <= 0, fmax <= fmin, negative duration).
     */
    void validate() const;

    float fmin = 62.5f;    ///< Lower frequency limit of calibration in Hz.
    float fmax = 4000.0f;  ///< Upper frequency limit of calibration in Hz.
    float duration = 1.0f; ///< Duration of the measurement stimulus in seconds.
    float prewait =
        0.125f; ///< Waiting time in seconds between stimulus onset and the
                ///< start of measurement to allow for transient settling.
    float reflevel = 80.0f; ///< Reference level in dB SPL for calibration.
    float bandsperoctave =
        3.0f; ///< Number of bands per octave used in the
              ///< filterbank for frequency-dependent level analysis.
    float bandoverlap =
        2.0f; ///< Overlap factor in bands for the analysis filterbank.
    uint32_t max_eqstages =
        0u; ///< Maximum number of biquad filter stages to use for frequency
            ///< compensation (0 means no frequency compensation).
    const bool issub =
        false; ///< True if this parameter set is for a subwoofer.
  };

  /**
     @brief Configuration container for a calibration session.

     This class holds all configuration data required to perform a calibration,
     including parameters for both speakers and subwoofers, hardware connections
     (microphone ports), and microphone sensitivity data.
   */
  class calib_cfg_t {
  public:
    calib_cfg_t();

    /**
       @brief Resets all parameters to factory defaults.
     */
    void factory_reset();

    /**
       @brief Reads configuration from global defaults.
     */
    void read_defaults();

    /**
       @brief Saves current configuration to global defaults.
     */
    void save_defaults();

    /**
       @brief Reads configuration from an XML layout node.
       @param layoutnode The XML node containing the session configuration.
     */
    void read_xml(const tsccfg::node_t& layoutnode);

    /**
       @brief Saves configuration to an XML layout node.
       @param layoutnode The XML node to write configuration to.
     */
    void save_xml(const tsccfg::node_t& layoutnode) const;

    /**
       @brief Validates the configuration.
       @throws TASCAR::ErrMsg if microphone ports are missing or mismatched with
               calibration data.
     */
    void validate() const;

    /**
       @brief Sets whether the layout includes subwoofers.
       @param h True if subwoofers are present.
     */
    void set_has_sub(bool h) { has_sub = h; };

    spk_eq_param_t
        par_speaker;        ///< Parameters for broadband speaker calibration.
    spk_eq_param_t par_sub; ///< Parameters for subwoofer calibration.
    std::vector<std::string>
        refport; ///< JACK port names of the measurement microphones.
    std::vector<float> miccalib; ///< Calibration offsets in dB for the
                                 ///< measurement microphones.
    bool initcal = true; ///< True if this is an initial calibration (false for
                         ///< recalibration).

  private:
    bool has_sub = false; ///< Internal flag indicating presence of subwoofers.
  };

  /**
     @brief Data structure holding the results of a speaker equalization
     measurement.

     This structure stores frequency response data, calculated equalization
     filters, and coherence metrics for a single speaker or subwoofer.
   */
  class spkeq_report_t {
  public:
    spkeq_report_t();

    /**
       @brief Constructor initializing the report with measurement data.
       @param label Label identifying the speaker (e.g., "spk1").
       @param vF Vector of frequency bins in Hz.
       @param vG_precalib Frequency response in dB before equalization.
       @param vG_postcalib Frequency response in dB after equalization.
       @param gain_db Overall gain correction applied in dB.
     */
    spkeq_report_t(std::string label, const std::vector<float>& vF,
                   const std::vector<float>& vG_precalib,
                   const std::vector<float>& vG_postcalib, float gain_db);

    std::string label;     ///< Identifier for the speaker.
    std::vector<float> vF; ///< Frequency vector (Hz).
    std::vector<float>
        vG_precalib; ///< Frequency response deviation before calibration (dB).
    std::vector<float>
        vG_postcalib; ///< Frequency response deviation after calibration (dB).
    float gain_db = 0.0f;    ///< Applied gain correction (dB).
    std::vector<float> eq_f; ///< Center frequencies of the EQ filters (Hz).
    std::vector<float> eq_g; ///< Gains of the EQ filters (dB).
    std::vector<float> eq_q; ///< Q-factors of the EQ filters.
    std::vector<float> level_db_re_fs; ///< Measured broadband level relative to
                                       ///< full scale (dB).
    std::vector<float> coh; ///< Coherence values for the measurement.
  };

  /**
     @brief A specialized TASCAR session for calibrating loudspeaker layouts.

     This class creates a JACK-based TASCAR session configured for automated
     speaker calibration. It handles the playback of test signals (pink noise),
     recording from measurement microphones, and calculation of level and
     frequency response corrections. The calibrated layout can be saved back
     to the XML file.
   */
  class calibsession_t : public TASCAR::session_t {
  public:
    /**
       @brief Constructs a calibration session.
       @param fname Path to the speaker layout XML file.
       @param cfg Configuration parameters for the calibration.
     */
    calibsession_t(const std::string& fname, const calib_cfg_t& cfg);

    ~calibsession_t();

    /**
       @brief Returns the current calibration level in dB SPL.
     */
    double get_caliblevel() const;

    /**
       @brief Returns the current diffuse field gain in dB.
     */
    double get_diffusegain() const;

    /**
       @brief Sets the absolute calibration level.
       @param level Desired level in dB SPL.
     */
    void set_caliblevel(float level);

    /**
       @brief Sets the absolute diffuse field gain.
       @param gain Desired gain in dB.
     */
    void set_diffusegain(float gain);

    /**
       @brief Increments the calibration level by a relative amount.
       @param dl Level change in dB.
     */
    void inc_caliblevel(float dl);

    /**
       @brief Increments the diffuse field gain by a relative amount.
       @param dl Gain change in dB.
     */
    void inc_diffusegain(float dl);

    /**
       @brief Activates or deactivates the point source (broadband speaker)
       calibration path.
       @param b True to activate point source, false to mute.
     */
    void set_active(bool b);

    /**
       @brief Activates or deactivates the diffuse field calibration path.
       @param b True to activate diffuse field, false to mute.
     */
    void set_active_diff(bool b);

    /**
       @brief Performs the measurement of relative speaker levels.

       This method iterates through all speakers and subwoofers, plays the
       configured stimulus, records the response, and calculates the necessary
       gain adjustments and equalization filters.
     */
    void get_levels();

    /**
       @brief Resets all measured levels and gains to their default
       (uncalibrated) state.
     */
    void reset_levels();

    /**
       @brief Saves the calibrated layout to a specific file.
       @param fname Destination filename.
     */
    void file_saveas(const std::string& fname);

    /**
       @brief Saves the calibrated layout to the original file.
     */
    void file_save();

    /**
       @brief Checks if the calibration process is fully complete.
       @return True if levels are recorded and both point source and diffuse
               calibration are active.
     */
    bool complete() const
    {
      return levelsrecorded && calibrated && calibrated_diff;
    };

    /**
       @brief Checks if the session has unsaved modifications.
       @return True if any levels, gains, or calibration states have changed.
     */
    bool modified() const
    {
      return levelsrecorded || calibrated || calibrated_diff || gainmodified;
    };

    /**
       @brief Returns the name of the layout file.
     */
    std::string name() const { return spkname; };

    /**
       @brief Returns the minimum measured level among all speakers (dB).
     */
    double get_lmin() const { return lmin; };

    /**
       @brief Returns the maximum measured level among all speakers (dB).
     */
    double get_lmax() const { return lmax; };

    /**
       @brief Returns the mean measured level of all speakers (dB).
     */
    double get_lmean() const { return lmean; };

    /**
       @brief Checks if speaker equalization measurements have been performed.
       @return True if levels have been recorded.
     */
    bool complete_spk_equal() const { return levelsrecorded; };

    /**
       @brief Returns the number of broadband speakers in the layout.
     */
    size_t get_num_bb() const
    {
      if(spk_file)
        return spk_file->size();
      return 0u;
    };

    /**
       @brief Returns the number of subwoofers in the layout.
     */
    size_t get_num_sub() const
    {
      if(spk_file)
        return spk_file->subs.size();
      return 0u;
    };

    /**
       @brief Returns the total number of output channels (speakers + subs).
     */
    size_t get_num_channels() const { return get_num_bb() + get_num_sub(); };

    /**
       @brief Estimates the total duration required for the calibration
       measurement.
       @return Estimated time in seconds.
     */
    double get_measurement_duration() const
    {
      return (cfg_.par_speaker.duration + cfg_.par_speaker.prewait) *
                 (double)get_num_bb() *
                 (1.0 + (double)(cfg_.par_speaker.max_eqstages > 0u)) +
             (cfg_.par_sub.duration + cfg_.par_sub.prewait) *
                 (double)get_num_sub() *
                 (1.0 + (double)(cfg_.par_sub.max_eqstages > 0u)) +
             0.2f * (float)cfg_.par_speaker.max_eqstages *
                 (double)get_num_bb() +
             0.2f * (float)cfg_.par_sub.max_eqstages * (double)get_num_sub();
    };

    /**
       @brief Returns the current speaker layout object.
       @return Reference to the speaker array.
     */
    const spk_array_diff_render_t& get_current_layout() const;

    /**
       @brief Enables or disables the frequency correction filters for the
       specific receiver.
       @param b True to apply EQ, false to bypass.
     */
    void enable_spkcorr_spec(bool b);

  private:
    bool gainmodified;    ///< True if gains were manually adjusted.
    bool levelsrecorded;  ///< True if get_levels() has been called.
    bool calibrated;      ///< True if point source calibration is active.
    bool calibrated_diff; ///< True if diffuse field calibration is active.
    double startlevel;    ///< Initial calibration level read from file.
    double startdiffgain; ///< Initial diffuse gain read from file.
    double delta;         ///< Accumulated change in calibration level.
    double delta_diff;    ///< Accumulated change in diffuse gain.
    double previous_delta_diff =
        0.0;             ///< Previous diffuse gain delta for validation.
    std::string spkname; ///< Filename of the speaker layout.
    spk_array_diff_render_t* spk_file =
        NULL; ///< Pointer to the speaker layout object.
    TASCAR::Scene::receiver_obj_t* rec_nsp =
        NULL; ///< Pointer to the NSP (non-specific) receiver.
    TASCAR::Scene::receiver_obj_t* rec_spec =
        NULL; ///< Pointer to the layout-specific receiver.
    TASCAR::receivermod_base_speaker_t* spk_nsp =
        NULL; ///< NSP speaker array object.
    TASCAR::receivermod_base_speaker_t* spk_spec =
        NULL;                     ///< Specific speaker array object.
    std::vector<float> levels;    ///< Measured levels of broadband speakers.
    std::vector<float> sublevels; ///< Measured levels of subwoofers.

  public:
    std::vector<float>
        levelsfrg; ///< Frequency-dependent level range (max-min) for speakers.
    std::vector<float>
        sublevelsfrg; ///< Frequency-dependent level range (max-min) for subs.

    TASCAR::Scene::route_t* levelroute =
        NULL; ///< Pointer to the routing module for level metering.

  private:
    const calib_cfg_t cfg_; ///< Configuration object.
    float lmin;             ///< Minimum measured level.
    float lmax;             ///< Maximum measured level.
    float lmean;            ///< Mean measured level.
    std::string calibfor;   ///< String describing the calibration target (e.g.,
                            ///< "type:nsp").
    jackrec2wave_t jackrec; ///< JACK recording interface.
    std::vector<TASCAR::wave_t>
        bbrecbuf; ///< Recording buffers for broadband stimuli.
    std::vector<TASCAR::wave_t>
        subrecbuf;               ///< Recording buffers for subwoofer stimuli.
    TASCAR::wave_t teststim_bb;  ///< Test stimulus buffer for broadband.
    TASCAR::wave_t teststim_sub; ///< Test stimulus buffer for subwoofer.
    bool isactive_pointsource =
        false;                     ///< State of the point source activation.
    bool isactive_diffuse = false; ///< State of the diffuse field activation.

  public:
    std::vector<spkeq_report_t> spkeq_report; ///< Vector containing measurement
                                              ///< reports for all speakers.
  };

  /**
     @brief High-level controller for the speaker calibration workflow.

     This class manages the state machine of the calibration process, guiding
     the user through steps such as file selection, configuration, measurement,
     and saving. It acts as a wrapper around `calibsession_t`.
   */
  class spkcalibrator_t {
  public:
    spkcalibrator_t();
    ~spkcalibrator_t();

    /**
       @brief Loads a speaker layout file for calibration.
       @param name Path to the XML layout file.
       @throws TASCAR::ErrMsg if a calibration is already running.
     */
    void set_filename(const std::string&);

    /**
       @brief Returns the filename of the current layout.
     */
    std::string get_filename() const { return filename; };

    /**
       @brief Returns a string description of the original (unmodified) speaker
       layout.
     */
    std::string get_orig_speaker_desc() const;

    /**
       @brief Returns a string description of the current (modified) speaker
       layout.
     */
    std::string get_current_speaker_desc() const;

    /**
       @brief Returns the measurement reports for all speakers.
       @return Vector of spkeq_report_t objects.
     */
    std::vector<spkeq_report_t> get_speaker_report() const
    {
      if(p_session)
        return p_session->spkeq_report;
      return {};
    };

    /**
       @brief Step 1: Confirm file selection.
       Resets the state machine to the initial state after a file is selected.
     */
    void step1_file_selected();

    /**
       @brief Step 2: Confirm configuration revision.
       Initializes the internal calibration session with the current
       configuration.
    */
    void step2_config_revised();

    /**
       @brief Step 3: Confirm calibration initialization.
       Prepares the session for measurement, typically disabling EQ filters
       for initial raw measurements.
     */
    void step3_calib_initialized();

    /**
       @brief Step 4: Confirm speaker equalization.
       Enables the calculated EQ filters in the specific receiver to verify
       the frequency response correction.
     */
    void step4_speaker_equalized();

    /**
       @brief Step 5: Confirm level adjustment.
       Marks the process as ready for saving. This implies that the reference
       level and diffuse gain have been adjusted to the desired values.
     */
    void step5_levels_adjusted();

    /**
       @brief Reverts to the previous step in the workflow.
     */
    void go_back();

    calib_cfg_t cfg; ///< Public configuration object.

    /**
       @brief Access to the level meter of a specific input channel.
       @param k Channel index.
       @return Reference to the level meter object.
     */
    const TASCAR::levelmeter_t& get_meter(uint32_t k) const;

    /**
       @brief Activates or deactivates the point source.
       @param act True to activate.
     */
    void set_active_pointsource(bool act)
    {
      if(p_session)
        p_session->set_active(act);
    };

    /**
       @brief Activates or deactivates the diffuse field.
       @param act True to activate.
     */
    void set_active_diffuse(bool act)
    {
      if(p_session)
        p_session->set_active_diff(act);
    };

    /**
       @brief Increments the diffuse gain.
       @param d Change in dB.
     */
    void inc_diffusegain(float d)
    {
      if(p_session)
        p_session->inc_diffusegain(d);
    };

    /**
       @brief Increments the calibration level.
       @param d Change in dB.
     */
    void inc_caliblevel(float d)
    {
      if(p_session)
        p_session->inc_caliblevel(d);
    };

    /**
       @brief Sets the absolute calibration level.
       @param d Level in dB.
     */
    void set_caliblevel(float d)
    {
      if(p_session)
        p_session->set_caliblevel(d);
    };

    /**
       @brief Sets the absolute diffuse gain.
       @param g Gain in dB.
     */
    void set_diffusegain(float g)
    {
      if(p_session)
        p_session->set_diffusegain(g);
    };

    /**
       @brief Gets the current calibration level.
       @return Level in dB.
     */
    double get_caliblevel() const
    {
      if(p_session)
        return p_session->get_caliblevel();
      return 0;
    };

    /**
       @brief Gets the current diffuse gain.
       @return Gain in dB.
     */
    double get_diffusegain() const
    {
      if(p_session)
        return p_session->get_diffusegain();
      return 0;
    };

    /**
       @brief Gets the estimated measurement duration.
       @return Duration in seconds.
     */
    double get_measurement_duration() const
    {
      if(p_session)
        return p_session->get_measurement_duration();
      return 0;
    };

    /**
       @brief Triggers the level measurement process.
     */
    void get_levels()
    {
      if(p_session)
        p_session->get_levels();
    };

    /**
       @brief Resets measured levels.
     */
    void reset_levels()
    {
      if(p_session)
        p_session->reset_levels();
    };

    /**
       @brief Checks if speaker equalization is complete.
       @return True if levels are recorded.
     */
    bool complete_spk_equal() const
    {
      if(p_session)
        return p_session->complete_spk_equal();
      return false;
    };

    /**
       @brief Checks if the entire calibration workflow is complete.
       @return True if all steps are finalized.
     */
    bool complete() const
    {
      if(p_session)
        return p_session->complete();
      return false;
    };

    /**
       @brief Saves the calibrated layout to the file.
     */
    void save()
    {
      if(p_session)
        p_session->file_save();
    };

    /**
       @brief Reloads configuration settings from the layout XML file.
     */
    void cfg_load_from_layout()
    {
      if(p_layout_doc)
        cfg.read_xml(p_layout_doc->root());
    }

    /**
       @brief Checks if the layout contains subwoofers.
       @return True if subwoofers are present.
     */
    bool has_sub() const
    {
      if(p_layout)
        return p_layout->subs.size() > 0u;
      return false;
    }

  private:
    std::string filename;      ///< Path to the layout file.
    uint32_t currentstep = 0u; ///< Current step in the calibration workflow.
    calibsession_t* p_session =
        NULL; ///< Pointer to the active calibration session.
    xml_doc_t* p_layout_doc =
        NULL; ///< Pointer to the XML document of the layout.
    spk_array_diff_render_t* p_layout =
        NULL; ///< Pointer to the speaker layout object.
    TASCAR::levelmeter_t
        fallbackmeter; ///< Default meter returned if no session is active.
  };

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tags-mode: nil
 * compile-command: "make -C .."
 * End:
 */
