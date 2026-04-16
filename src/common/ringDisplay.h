#pragma once
#include "rack.hpp"

using namespace rack;

/**
 * BipolarModRingKnob
 * ------------------
 * Knob con anillo de modulación bipolar (CV).
 * - Soporta parámetros con cualquier rango (usa normalización real)
 * - Maneja inputs desconectados correctamente
 * - Render seguro con NanoVG (save/restore)
 * - Arco robusto sin glitches de dirección
 */
struct BipolarModRingKnob : app::SvgKnob {
    // Referencias al módulo
    Module* module = nullptr;
    int inputId = -1;
    int attenId = -1; // opcional

    // Estilo visual
    NVGcolor ringColor = nvgRGBA(0x28, 0xaf, 0xff, 0xff);
    float ringThickness = 1.5f;
    float ringOffset = 3.0f;

    // Rango de CV esperado (±5V típico en Rack)
    float cvRange = 5.0f;

    BipolarModRingKnob() {
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/knob.svg")
        ));
    }

    void draw(const DrawArgs& args) override {
        // Dibujar knob base
        app::SvgKnob::draw(args);

        if (!module || this->paramId < 0 || inputId < 0)
            return;

        ParamQuantity* pq = module->paramQuantities[this->paramId];
        if (!pq)
            return;

        // =========================
        // 1. Obtener valores reales del parámetro (igual que ModMatrix::process)
        // =========================
        float minVal = pq->getMinValue();
        float maxVal = pq->getMaxValue();
        float range = std::max(1e-6f, maxVal - minVal);  // Evitar división por cero
        float baseVal = pq->getValue();

        // 2. Obtener CV y Attenuverter
        float cv = module->inputs[inputId].isConnected()
            ? module->inputs[inputId].getVoltage()
            : 0.0f;

        float atten = (attenId >= 0)
            ? module->params[attenId].getValue()
            : 1.0f;

        // 3. Calcular la modulación real (IGUAL que en ModMatrix::process)
        float modulationAmount = (cv * atten / 10.0f) * range;

        // 4. Calcular el valor final real y normalizarlo
        float finalValue = baseVal + modulationAmount;
        float finalNorm = (finalValue - minVal) / range;
        finalNorm = clamp(finalNorm, 0.0f, 1.0f);

        // 5. Normalizar el valor base para el ángulo de inicio
        float baseNorm = (baseVal - minVal) / range;
        baseNorm = clamp(baseNorm, 0.0f, 1.0f);

        // =========================
        // 2. Conversión a ángulo
        // VCV Rack standard: -135° (min) a +135° (max) = 270° total
        // NanoVG: 0 es a la derecha (3 o'clock)
        // =========================
        auto valueToAngle = [](float v) {
           
             return (v * 1.5f * M_PI) - (0.75f * M_PI) - M_PI_2;
           
        };

        float startAngle = valueToAngle(baseNorm);
        float endAngle   = valueToAngle(finalNorm);
        

        if (std::abs(startAngle - endAngle) < 0.0001f)
            return;

        // =========================
        // 3. Geometría
        // NanoVG: ángulo 0 es derecha, crece en sentido horario
        // =========================
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float radius = std::min(box.size.x, box.size.y) * 0.5f + ringOffset;

        float a0 = startAngle;
        float a1 = endAngle;

        // =========================
        // 4. Render (NanoVG seguro)
        // =========================
        nvgSave(args.vg);

        nvgBeginPath(args.vg);

        // Dirección robusta
        int dir = (a1 > a0) ? NVG_CW : NVG_CCW;

        nvgArc(args.vg, cx, cy, radius, a0, a1, dir);

        nvgStrokeWidth(args.vg, ringThickness);
        nvgStrokeColor(args.vg, ringColor);

        // Glow
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStroke(args.vg);

        // Stroke normal
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgStroke(args.vg);

        nvgRestore(args.vg);
    }
};

struct TinyAttenuverterKnob : app::SvgKnob {
    TinyAttenuverterKnob() {
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/tiny_attenuverter_black.svg")
        ));
        box.size = Vec(14, 14);
        minAngle = -0.75f * M_PI;
        maxAngle = 0.75f * M_PI;
    }
};

struct JackIn : app::SvgPort {
    JackIn() {
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/jack_in.svg")
        ));
    }
};

struct JackOut : app::SvgPort {
    JackOut() {
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/jack_out.svg")
        ));
    }
};