#include "plugin.hpp"
#include "common/ringDisplay.h"

// Constantes para el osciloscopio
static const int SCOPE_BUFFER_SIZE = 256;

struct ScopePoint {
    float min = INFINITY;
    float max = -INFINITY;
};


#define NUMBER_OF_EQUATIONS 21
#define PITCH_MIN 3072.0f
#define PITCH_MAX 44100.0f

struct GlitchStormModule : Module
{
  uint8_t value = 0;     // value is the output of the equations
  float output = 0;  // output is the audio output
  int32_t t;  // t is the time counter used in the equations
  float phaseAccumulator = 0.0f;

  uint32_t a;
  uint32_t b;
  uint32_t c;

  // Osciloscopio
  ScopePoint scopeBuffer[SCOPE_BUFFER_SIZE];
  ScopePoint currentScopePoint;
  int scopeBufferIndex = 0;
  int scopeFrameIndex = 0;

  uint8_t aTop = 0; 
  uint8_t aBottom = 0; 
  uint8_t bTop = 0; 
  uint8_t bBottom = 0; 
  uint8_t cTop = 0; 
  uint8_t cBottom = 0; 

  enum ParamIds {
    CLOCK_DIVISION_KNOB,
    EQUATION_KNOB,
    PARAM_KNOB_1,
    PARAM_KNOB_2,
    PARAM_KNOB_3,
    PARAM_ATTENUATOR_1,
    PARAM_ATTENUATOR_2,  
    PARAM_ATTENUATOR_3,  
    SINE_SWITCH,
    OSCILLOSCOPE_ON,
    NUM_PARAMS
	};
	enum InputIds {
    PARAM_INPUT_1,
    PARAM_INPUT_2,
    PARAM_INPUT_3,
    EQUATION_INPUT,
    CLOCK_CV_INPUT,
    T_INPUT,
    SYNC_CLOCK_INPUT,
    RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
    AUDIO_MONO,
    AUDIO_LEFT,
    AUDIO_RIGHT,
    CLOCK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	json_t *dataToJson() override;
	void dataFromJson(json_t *root) override;
	float calculate_inputs(int input_index, int knob_index, float maximum_value);
	float calculate_parameter_input(int input_index, int knob_index, int attenuator_index, float maximum_value);
	void process(const ProcessArgs &args) override;
	float compute(uint32_t equation_number, uint32_t t, uint32_t a, uint32_t b, uint32_t c);
	uint32_t div(uint32_t a, uint32_t b);
	uint32_t mod(uint32_t a, uint32_t b);
	void updateReadouts() {}

	GlitchStormModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(EQUATION_KNOB, 0.0f, NUMBER_OF_EQUATIONS - 1, 6.0f, "Program");
		paramQuantities[EQUATION_KNOB]->snapEnabled = true;
		configParam(PARAM_KNOB_1, 0.0f, 15.0f, 7.0f, "A");
		paramQuantities[PARAM_KNOB_1]->snapEnabled = true;
		configParam(PARAM_KNOB_2, 0.0f, 15.0f, 7.0f, "B");
		paramQuantities[PARAM_KNOB_2]->snapEnabled = true;
		configParam(PARAM_KNOB_3, 0.0f, 15.0f, 10.0f, "C");
		paramQuantities[PARAM_KNOB_3]->snapEnabled = true;
		configParam(CLOCK_DIVISION_KNOB, PITCH_MIN, PITCH_MAX, 8000.0f, "Rate", " Hz");
		configParam(PARAM_ATTENUATOR_3, -1.f, 1.f, 0.f, "Atte C", "%", 0.f, 100.f);
		configParam(SINE_SWITCH, 0.0f, 1.0f, 1.0f, "Sine Switch");
    configParam(OSCILLOSCOPE_ON, 0.0f, 1.0f, 1.0f, "Oscilloscope On");
		configParam(PARAM_ATTENUATOR_2, -1.f, 1.f, 0.f, "Atte B", "%", 0.f, 100.f);
		configParam(PARAM_ATTENUATOR_1, -1.f, 1.f, 0.f, "Atte A", "%", 0.f, 100.f);
		configInput(PARAM_INPUT_1, "A CV");
    configInput(PARAM_INPUT_2, "B CV");
    configInput(PARAM_INPUT_3, "C CV");
    configInput(EQUATION_INPUT, "Program CV");
    configInput(CLOCK_CV_INPUT, "Rate CV");
    configInput(T_INPUT, "T input");
    configInput(SYNC_CLOCK_INPUT, "Sync clock");
    configInput(RESET_INPUT, "Reset");
		configOutput(AUDIO_MONO, "Mono");
    configOutput(AUDIO_LEFT, "Left");
    configOutput(AUDIO_RIGHT, "Right");
		configOutput(CLOCK_OUTPUT, "Clock");
	}
};

// Widget de display para el osciloscopio
struct BlankScopeDisplay : Widget {
    GlitchStormModule* module;

    void draw(const DrawArgs& args) override {
        if (!module) return;
        
        // Verificar si el osciloscopio está encendido
        if (module->params[GlitchStormModule::OSCILLOSCOPE_ON].getValue() < 0.5f) return;

        // Dibujar waveform
        nvgStrokeColor(args.vg, nvgRGBA(0xC0, 0x00, 0xC0, 0xff));
        nvgStrokeWidth(args.vg, 1.5);
        nvgBeginPath(args.vg);
        
        bool first = true;
        for (int i = 0; i < SCOPE_BUFFER_SIZE; i++) {
            float x = (float)i / (SCOPE_BUFFER_SIZE - 1) * box.size.x;
            float avg = (module->scopeBuffer[i].min + module->scopeBuffer[i].max) / 2.0f;
            // Mapear de [-5, 5] a [box.size.y, 0]
            float y = rescale(avg, -5.0f, 5.0f, box.size.y - 10, 2);
            
            if (first) {
                nvgMoveTo(args.vg, x, y);
                first = false;
            } else {
                nvgLineTo(args.vg, x, y);
            }
        }
        nvgStroke(args.vg);
    }
};

struct GlitchStormModuleWidget : ModuleWidget
{
    GlitchStormModuleWidget(GlitchStormModule *module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/glitch_traced.svg")));

        //addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        //addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        //addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        //addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // --- SECCIÓN EQUATION (Fila 1) ---
        BipolarModRingKnob* equation_knob = createParamCentered<BipolarModRingKnob>(mm2px(Vec(13.0, 23.0)), module, GlitchStormModule::EQUATION_KNOB);
        equation_knob->snap = true;
        equation_knob->minAngle = -0.75f * M_PI;
        equation_knob->maxAngle = 0.75f * M_PI;
        equation_knob->module = module;
        equation_knob->inputId = GlitchStormModule::EQUATION_INPUT;
        equation_knob->ringColor = nvgRGB(0xff, 0xd7, 0x00); // Dorado
        addParam(equation_knob);

        addInput(createInputCentered<JackiesPort>(mm2px(Vec(28.0, 24.0)), module, GlitchStormModule::EQUATION_INPUT));

        // --- SECCIÓN PITCH ---
        BipolarModRingKnob* clock_knob = createParamCentered<BipolarModRingKnob>(mm2px(Vec(48.0, 23.0)), module, GlitchStormModule::CLOCK_DIVISION_KNOB);
        clock_knob->minAngle = -0.75f * M_PI;
        clock_knob->maxAngle = 0.75f * M_PI;
        clock_knob->module = module;
        clock_knob->inputId = GlitchStormModule::CLOCK_CV_INPUT;
        clock_knob->ringColor = nvgRGB(0xff, 0xd7, 0x00); // Dorado
        addParam(clock_knob);
        addInput(createInputCentered<JackiesPort>(mm2px(Vec(64.0, 24.0)), module, GlitchStormModule::CLOCK_CV_INPUT));

        // --- SECCIÓN PARAMETERS ---
        float knobY = 74.0+1;

        BipolarModRingKnob* knob_1 = createParamCentered<BipolarModRingKnob>(mm2px(Vec(10.0+7, knobY)), module, GlitchStormModule::PARAM_KNOB_1);
        knob_1->snap = true;
        knob_1->minAngle = -0.75f * M_PI;
        knob_1->maxAngle = 0.75f * M_PI;
        knob_1->module = module;
        knob_1->inputId = GlitchStormModule::PARAM_INPUT_1;
        knob_1->attenId = GlitchStormModule::PARAM_ATTENUATOR_1;
        knob_1->ringColor = nvgRGB(0xff, 0xd7, 0x00); // Dorado
        addParam(knob_1);

        BipolarModRingKnob* knob_2 = createParamCentered<BipolarModRingKnob>(mm2px(Vec(31.0+7, knobY)), module, GlitchStormModule::PARAM_KNOB_2);
        knob_2->snap = true;
        knob_2->minAngle = -0.75f * M_PI;
        knob_2->maxAngle = 0.75f * M_PI;
        knob_2->module = module;
        knob_2->inputId = GlitchStormModule::PARAM_INPUT_2;
        knob_2->attenId = GlitchStormModule::PARAM_ATTENUATOR_2;
        knob_2->ringColor = nvgRGB(0xff, 0xd7, 0x00); // Dorado
        addParam(knob_2);

        BipolarModRingKnob* knob_3 = createParamCentered<BipolarModRingKnob>(mm2px(Vec(52.0+7, knobY)), module, GlitchStormModule::PARAM_KNOB_3);
        knob_3->snap = true;
        knob_3->minAngle = -0.75f * M_PI;
        knob_3->maxAngle = 0.75f * M_PI;
        knob_3->module = module;
        knob_3->inputId = GlitchStormModule::PARAM_INPUT_3;
        knob_3->attenId = GlitchStormModule::PARAM_ATTENUATOR_3;
        knob_3->ringColor = nvgRGB(0xff, 0xd7, 0x00); // Dorado
        addParam(knob_3);

        // --- SECCIÓN TRIMPOTS (Fila 3: Atenuadores con escala 0...1) ---
        float trimY = 87.0; // Situados en los círculos con escala
        addParam(createParamCentered<TinyAttenuverterKnob>(mm2px(Vec(16.5, trimY)), module, GlitchStormModule::PARAM_ATTENUATOR_1));
        addParam(createParamCentered<TinyAttenuverterKnob>(mm2px(Vec(38.0, trimY)), module, GlitchStormModule::PARAM_ATTENUATOR_2));
        addParam(createParamCentered<TinyAttenuverterKnob>(mm2px(Vec(59.0, trimY)), module, GlitchStormModule::PARAM_ATTENUATOR_3)); 

        // --- OSCILOSCOPIO ---
        BlankScopeDisplay* scopeDisplay = createWidget<BlankScopeDisplay>(mm2px(Vec(5.0, 35.0)));
        scopeDisplay->box.size = mm2px(Vec(66.0, 14.0));
        scopeDisplay->module = module;
        addChild(scopeDisplay);

         // --- SINE SWITCH ---
        addParam(createParamCentered<CKSS>(mm2px(Vec(42.0, 62.0)), module, GlitchStormModule::SINE_SWITCH));

        // --- OSCILLOSCOPE ON/OFF SWITCH ---
        addParam(createParamCentered<CKSS>(mm2px(Vec(8.0, 62.0)), module, GlitchStormModule::OSCILLOSCOPE_ON));

        // --- SECCIÓN JACKS A, B, C (Fila 4) ---
        float jackY = 97.0; 
        addInput(createInputCentered<JackiesPort>(mm2px(Vec(17.0, jackY)), module, GlitchStormModule::PARAM_INPUT_1));
        addInput(createInputCentered<JackiesPort>(mm2px(Vec(38.0, jackY)), module, GlitchStormModule::PARAM_INPUT_2));
        addInput(createInputCentered<JackiesPort>(mm2px(Vec(59.0, jackY)), module, GlitchStormModule::PARAM_INPUT_3));
float bottom_jacksY = 117.5; 
        // --- SALIDA CLOCK ---
        addOutput(createOutputCentered<JackiesPort>(mm2px(Vec(8.5+3, bottom_jacksY)), module, GlitchStormModule::CLOCK_OUTPUT));

        // --- RESET INPUT ---
        addInput(createInputCentered<JackiesPort>(mm2px(Vec(24.5, bottom_jacksY)), module, GlitchStormModule::RESET_INPUT));

        // --- SALIDA DE AUDIO (OUT) ---
        addOutput(createOutputCentered<JackiesPort>(mm2px(Vec(61.5+3, bottom_jacksY)), module, GlitchStormModule::AUDIO_MONO));
        addOutput(createOutputCentered<JackiesPort>(mm2px(Vec(34.5+3, bottom_jacksY)), module, GlitchStormModule::AUDIO_LEFT));
        addOutput(createOutputCentered<JackiesPort>(mm2px(Vec(48.5+3, bottom_jacksY)), module, GlitchStormModule::AUDIO_RIGHT));
    }
};

// Autosave module data.  VCV Rack decides when this should be called.
json_t *GlitchStormModule::dataToJson()
	{
		json_t *root = json_object();
  	return root;
	}

	// Load module data
void GlitchStormModule::dataFromJson(json_t *root)
	{
    // There's nothing to save and load yet.
	}

  // This is a helper function for reading inputs with attenuators
  //
  // input_index: The index of the input in InputIds.
  // knob_index: The index of the parameter in ParamIds
  // maxiumum value: The output of calculation_inputs will range from 0 to maximum_value
  //
  // Example call:
  //
  // float value = calculate_inputs(MY_INPUT, MY_ATTENUATOR_KNOB, 1200.0);
  //

float GlitchStormModule::calculate_inputs(int input_index, int knob_index, float maximum_value)
  {
    float input_value = inputs[input_index].getVoltage() / 10.0;
    float knob_value = params[knob_index].getValue();
    float out = 0;

    if(inputs[input_index].isConnected())
    {
      input_value = clamp(input_value, 0.0, 1.0);
      out = clamp((input_value * maximum_value) + (knob_value * maximum_value), 0.0, maximum_value);
    }
    else
    {
      out = clamp(knob_value * maximum_value, 0.0, maximum_value);
    }
    return(out);
  }

float GlitchStormModule::calculate_parameter_input(int input_index, int knob_index, int attenuator_index, float maximum_value)
  {
    float input_value = inputs[input_index].getVoltage() / 10.0 * maximum_value; // Scale input to 0-128 range
    float knob_value = params[knob_index].getValue();
    float attenuator_value = params[attenuator_index].getValue();
    
    float cv_contribution = 0.0f;
    if (inputs[input_index].isConnected()) {
        cv_contribution = input_value * attenuator_value;
    }
    
    float out = clamp(knob_value + cv_contribution, 0.0f, maximum_value);
    
    return out;
  }


void GlitchStormModule::process(const ProcessArgs &args)
	{
    if(inputs[RESET_INPUT].isConnected() && inputs[RESET_INPUT].getVoltage() > 1.0f) {
      t = 0;
      phaseAccumulator = 0.0f;
    }

    if(inputs[T_INPUT].isConnected())
    {
      t = inputs[T_INPUT].getVoltage() * 2048;
    }
    else
    {
      // Pitch control usando phase accumulator (4096 - 44100 Hz)
      float pitch = params[CLOCK_DIVISION_KNOB].getValue();
      
      // Añadir modulación CV si está conectada
      if(inputs[CLOCK_CV_INPUT].isConnected()) {
        float cv = inputs[CLOCK_CV_INPUT].getVoltage() / 10.0f;
        pitch = PITCH_MIN + cv * (PITCH_MAX - PITCH_MIN);
        pitch = clamp(pitch, PITCH_MIN, PITCH_MAX);
      }
      
      // Phase accumulator: incrementa t cuando la fase supera 1.0
      phaseAccumulator += pitch / args.sampleRate;
      
      while(phaseAccumulator >= 1.0f) {
        t = t + 1;
        phaseAccumulator -= 1.0f;
      }
    }

    //
    // Read equation, parameter, and expression inputs.
    // Lots of implicit float to int conversion happening here

    uint32_t equation = params[EQUATION_KNOB].getValue() + ((inputs[EQUATION_INPUT].getVoltage() / 10.0) * (float) NUMBER_OF_EQUATIONS);
    equation = clamp((int)equation, 0, NUMBER_OF_EQUATIONS - 1);

    // Actualizar rangos de knobs según ecuación
    int aT, bT, cT;
    /*
    switch(equation) {
        case 0: aT=10; bT=28; cT=10;  break;
        case 1: aT=12; bT=20; cT=12;  break;
        case 2: aT=30; bT=16; cT=10;  break;
        case 3: aT=12; bT=16; cT=10;  break;
        case 4: aT=24; bT=22; cT=16;  break;
        case 5: aT=10; bT=14; cT=14;  break;
        case 6: aT=10; bT=22; cT=8;   break;
        case 7: aT=12; bT=20; cT=20;  break;
        case 8: aT=16; bT=86; cT=26;  break;
        case 9: aT=8;  bT=22; cT=13;  break;
        case 10: aT=16; bT=22; cT=9;  break;
        case 11: aT=18; bT=8;  cT=14;  break;
        case 12: aT=18; bT=14; cT=10; break;
        case 13: aT=8;  bT=16; cT=1;  break;
        case 14: aT=8;  bT=9;  cT=5;  break;
        case 15: aT=8;  bT=9;  cT=6;  break;
        default: aT=31; bT=127; cT=31; break;
    }
        */
        aT=15; bT=15; cT=15;
    paramQuantities[PARAM_KNOB_1]->minValue = 0;
    paramQuantities[PARAM_KNOB_1]->maxValue = 15;
    paramQuantities[PARAM_KNOB_2]->minValue = 0;
    paramQuantities[PARAM_KNOB_2]->maxValue = 15;
    paramQuantities[PARAM_KNOB_3]->minValue = 0;
    paramQuantities[PARAM_KNOB_3]->maxValue = 15;

    a = calculate_parameter_input(PARAM_INPUT_1, PARAM_KNOB_1, PARAM_ATTENUATOR_1, (float)aT);
    b = calculate_parameter_input(PARAM_INPUT_2, PARAM_KNOB_2, PARAM_ATTENUATOR_2, (float)bT);
    c = calculate_parameter_input(PARAM_INPUT_3, PARAM_KNOB_3, PARAM_ATTENUATOR_3, (float)cT);

    updateReadouts();

    // Calcular señal base (mono)
    float monoSignal = (compute(equation, t, a, b, c) * 10.0) - 5.0;
    outputs[AUDIO_MONO].setVoltage(monoSignal);
    
    // Capturar señal para el osciloscopio
    currentScopePoint.min = std::min(currentScopePoint.min, monoSignal);
    currentScopePoint.max = std::max(currentScopePoint.max, monoSignal);
    
    // Cada ciertos frames, empujar al buffer
    scopeFrameIndex++;
    if (scopeFrameIndex >= 4) {  // Capturar cada 4 samples (~10kHz a 44.1kHz)
        scopeFrameIndex = 0;
        if (scopeBufferIndex < SCOPE_BUFFER_SIZE) {
            scopeBuffer[scopeBufferIndex] = currentScopePoint;
            scopeBufferIndex++;
        }
        currentScopePoint = ScopePoint();
    }
    
    // Reset del buffer cuando se llena (loop)
    if (scopeBufferIndex >= SCOPE_BUFFER_SIZE) {
        scopeBufferIndex = 0;
    }
    
    // Stereo panning basado en value (0-255)
    // value=128: ambos canales al 50%
    // valor bajo: más L, valor alto: más R
    float normValue = (float)value / 255.0f;  // 0.0 a 1.0
    float leftGain = 1.0f - normValue;   // 1.0 a 0.0
    float rightGain = normValue;         // 0.0 a 1.0
    
    // Ajustar para que en el centro (128) ambos estén al 50%
    leftGain = clamp(leftGain * 2.0f, 0.0f, 1.0f);
    rightGain = clamp(rightGain * 2.0f, 0.0f, 1.0f);
    
    outputs[AUDIO_LEFT].setVoltage(monoSignal * leftGain);
    outputs[AUDIO_RIGHT].setVoltage(monoSignal * rightGain);

    // Clock output: pulso cuando t % 256 == 0
    if (t % 1024 == 0) {
        outputs[CLOCK_OUTPUT].setVoltage(10.0f);
    } else {
        outputs[CLOCK_OUTPUT].setVoltage(0.0f);
    }


  }

float GlitchStormModule::compute(uint32_t equation_number, uint32_t t, uint32_t a, uint32_t b, uint32_t c)
{
    uint8_t _a = a % 16; 
    uint8_t _b = b % 16;
    uint8_t _c = c % 16;
    a=_a;
    b=_b;
    c=_c;

   switch(equation_number) {
      case 0:
  // basic fluor
   //value = t >> b & t ? t >> a : t >> c;
   value = ((t * (2 - (1 & (-t >> 11))) * (c + (2 & (t >> 14)))) >> (3 & (t >> a))) | (t >> b);

      aTop = 10;
      aBottom = 0;
      bTop = 22;
      bBottom = 10;
      cTop = 8;
      cBottom = 0;
      break;
    case 1:
    value = (((t & ((t >> a))) + (t | ((t >> b)))) & (t >> (c + 1))) | ((t >> a) & (t * (t >> b)));
    aTop = 10;
    aBottom = 0;
    bTop = 14;
    bBottom = 0;
    cTop = 14;
    cBottom = 0;
    break;

  case 2://OK
   value = ((t * ((t & 16384) ? 7 : 5) * (3 + (c & (t >> a)))) >> (c & (t >> b))) | (t >> 6);
    aTop = 15; aBottom = 0; bTop = 15; bBottom = 0; cTop = 15; cBottom = 0;
    break;

  case 3:
    value = ((t >> c) ^ (t & 37)) | ((t + (t ^ (t >> a)) - t * (((t >> a) ? 2 : 6) & (t >> b))) ^ ((t << 1) & ((t & b) ? (t >> 4) : (t >> 10))));
    aTop = 30;
    aBottom = 6;
    bTop = 16;
    bBottom = 0;
    cTop = 10;
    cBottom = 0;
    break;

  case 4:
  
    value = (((b * t) >> a) ^ (t & (37 - c))) | (((t + ((t ^ (t >> 11)))) - t * (((t >> 6) ? 2 : a) & (t >> (c + b)))) ^ ((t << 1) & ((t & 6) ? (t >> 4) : (t >> c))));
    aTop = 12;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 10;
    cBottom = 0;
    break;

  case 5:
    value = (((c * t) >> 2) ^ (t & (30 - b))) | (((t + ((t ^ (t >> b)))) - t * (((t >> 6) ? a : c) & (t >> a))) ^ ((t << 1) & ((t & b) ? (t >> 4) : (t >> c))));
    aTop = 24;
    aBottom = 0;
    bTop = 22;
    bBottom = 0;
    cTop = 16;
    cBottom = 0;
    break;

  case 6:
    value = (((t >> a) & t) - (t >> a) + ((t >> a) & t)) + (t * ((t >> c) & b));
    aTop = 10;
    aBottom = 3;
    bTop = 28;
    bBottom = 0;
    cTop = 14;
    cBottom = 3;
    break;

  case 7://XX
    value = ((~t) >> a) * ((2 + (42 & 2 * t * (7 & (t >> b)))) < (24 & (t * ((3 & (t >> c)) + 2))));
    aTop = 15; aBottom = 0; bTop = 15; bBottom = 0; cTop = 15; cBottom = 0;
    break;

  case 8://XX
    value = (t >> a) | ((0xC098C098 >> (t >> b)) * t) | (t >> c);
    aTop = 15; aBottom = 0; bTop = 15; bBottom = 0; cTop = 15; cBottom = 0;
    break;

  case 9:
    value = ((t * ((t >> a) | (t >> (a & c)))) & b & (t >> 8)) ^ ((t & (t >> c)) | (t >> 6));
    aTop = 16;
    aBottom = 0;
    bTop = 86;
    bBottom = 0;
    cTop = 26;
    cBottom = 0;
    break;

  case 10:
    value = (((t >> c) * 7) | ((t >> a) * 8) | ((t >> b) * 7)) & (t >> 7);
    aTop = 8;
    aBottom = 0;
    bTop = 22;
    bBottom = 0;
    cTop = 13;
    cBottom = 0;
    break;

  case 11:
    value = ((((t + 1) & (t >> a)) + (t >> b)) & ((t + 3) >> c));
    aTop = 3;
    aBottom = 12;
    bTop = 8;
    bBottom = 3;
    cTop = 6;
    cBottom = 1;
    break;

  case 12://XX
    value = (mod((t >> a), b) * t) & (t >> c);
    aTop = 15; aBottom = 0; bTop = 15; bBottom = 0; cTop = 15; cBottom = 0;
    break;

  case 13:
    value = (((t & (t / 6) >> b)) * (16382 + c) * b) + ((t >> 4)) * a;
    aTop = 8;
    aBottom = 0;
    bTop = 12;
    bBottom = 4;
    cTop = 6;
    cBottom = 0;
    break;

  case 14:
    value = ((t >> c) ^ (t & 1)) | ((t + (t ^ (t >> 21)) - t * (((t >> 4) ? b : a) & (t >> (12 - (a >> 1))))) ^ ((t << 1) & ((a & 12) ? (t >> 4) : (t >> 10))));
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;

  case 15://XX
  {
   int x = t & (t >> b);
    value = (int)(cos(t * 3.14159 / 512 * ((t & (c >> a)) + x) / x) * 256);
    aTop = 15; aBottom = 0; bTop = 15; bBottom = 0; cTop = 15; cBottom = 0;
    break;
  }

    case 16://TESTING
    value = ((5 >> ((t >> 12) + (t >> b))) & t) ? (t >> a) : (t >> (t >> c));
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;

    case 17://TESTING
     value = ((t * ((t >> a) | (t >> (a & c)))) & b & (t >> 8)) ^ ((t & (t >> c)) | (t >> 6));
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;


    
case 18://TESTING
    value = (((t << a) * ((t & 16384) ? 6 : 5) * (4 - (a & (t >> 8)))) >> (b & (-(t * 8)) >> c)) | (t >> 6);
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;
case 19://TESTING
    value = (((t * 12) & (t >> a)) | ((t * b) & (t >> c)) | ((t * b) & (c / (b << 2)))) - 2;
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;
    case 20://TESTING
    value = (((t << 2) ^ (t >> 4) ^ ((t << a) & (t >> b))) | ((t << 1) & ((-t) >> c)));
    aTop = 8;
    aBottom = 0;
    bTop = 16;
    bBottom = 0;
    cTop = 1;
    cBottom = 6;
    break;

    }
    bool sine_switch = params[SINE_SWITCH].getValue() > 0.5f;
    if (sine_switch) {
        float phase = (float)(value & 255) / 256.0f;
        return sinf(2.0f * M_PI * phase);
    } else { 
        return (float)value / 127.5f - 1.0f;
    }
}

uint32_t GlitchStormModule::div(uint32_t a, uint32_t b)
  {
    if(b == 0) return(0);
    return(a / b);
  }

uint32_t GlitchStormModule::mod(uint32_t a, uint32_t b)
  {
    if(b == 0) return(0);
    return(a % b);
  }

Model *modelGlitchStorm = createModel<GlitchStormModule, GlitchStormModuleWidget>("GlitchStorm");
Model *modelBlank = createModel<GlitchStormModule, GlitchStormModuleWidget>("blank");
