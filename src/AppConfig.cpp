#include "AppConfig.hpp"
#include <tinyxml2.h>
#include <stdexcept>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

using namespace tinyxml2;

static const char* reqText(XMLElement* el, const char* child) {
    auto* c = el->FirstChildElement(child);
    if (!c || !c->GetText())
        throw std::runtime_error(std::string("missing <") + child + ">");
    return c->GetText();
}

static std::string optText(XMLElement* el, const char* child,
                           const char* def = "") {
    auto* c = el->FirstChildElement(child);
    return (c && c->GetText()) ? c->GetText() : def;
}

static double optD(XMLElement* el, const char* child, double def) {
    auto* c = el->FirstChildElement(child);
    if (!c || !c->GetText()) return def;
    return std::stod(c->GetText());
}

static int64_t optI64(XMLElement* el, const char* child, int64_t def) {
    auto* c = el->FirstChildElement(child);
    if (!c || !c->GetText()) return def;
    return std::stoll(c->GetText());
}

AppConfig AppConfig::fromXml(const std::string& path) {
    XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != XML_SUCCESS)
        throw std::runtime_error("Cannot open config: " + path);

    auto* root = doc.FirstChildElement("demod_config");
    if (!root) throw std::runtime_error("Missing <demod_config> root element");

    AppConfig cfg;

    if (auto* b = root->FirstChildElement("broker")) {
        cfg.broker.url            = optText(b, "url", "amqp://localhost:5672");
        cfg.broker.username       = optText(b, "username");
        cfg.broker.password       = optText(b, "password");
        cfg.broker.demod_request_queue = optText(b, "demod_request_queue", "rf.demod.request");
        cfg.broker.task_queue     = optText(b, "task_request_queue", "sdr.tasks");
        cfg.broker.demod_topic    = optText(b, "demod_topic", "rf.demod");
    }

    if (auto* s = root->FirstChildElement("streaming"))
        cfg.local_ip = optText(s, "local_ip", "127.0.0.1");

    if (auto* o = root->FirstChildElement("output")) {
        cfg.output.output_dir   = optText(o, "output_dir", "/tmp/sdr-demod");
        cfg.output.publish_amqp = optText(o, "publish_amqp", "true") == std::string("true");
    }

    if (auto* e = root->FirstChildElement("engine")) {
        cfg.engine.rank             = (int)optD(e, "rank", 3);
        cfg.engine.audio_duration   = au::seconds(optI64(e, "audio_duration_ms",   5000) / 1000.0);
        cfg.engine.digital_duration = au::seconds(optI64(e, "digital_duration_ms", 2000) / 1000.0);
        cfg.engine.audio_sample_rate = au::hertz(optD(e, "audio_sample_rate_hz", 48000.0));
        if (auto* c = e->FirstChildElement("min_confidence"))
            cfg.engine.min_confidence = (float)optD(e, "min_confidence", 0.70);
    }

    // ── Threat detection ─────────────────────────────────────────────────────
    if (auto* th = root->FirstChildElement("threat")) {
        auto boolEl = [&](const char* tag, bool def) -> bool {
            std::string v = optText(th, tag, def ? "true" : "false");
            return (v == "true" || v == "1" || v == "yes");
        };
        cfg.threat.enabled          = boolEl("enabled",          false);
        cfg.threat.adsb_enabled     = boolEl("adsb_enabled",     true);
        cfg.threat.ais_enabled      = boolEl("ais_enabled",      true);
        cfg.threat.eas_enabled      = boolEl("eas_enabled",      true);
        cfg.threat.dsc_enabled      = boolEl("dsc_enabled",      true);
        cfg.threat.p25_rogue_enabled= boolEl("p25_rogue_enabled",true);
        cfg.threat.alert_topic      = optText(th, "alert_topic", "rf.alerts");
    }

    return cfg;
}

} // namespace demod
