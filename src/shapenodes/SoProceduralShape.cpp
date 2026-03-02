/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*!
  \class SoProceduralShape SoProceduralShape.h Inventor/nodes/SoProceduralShape.h
  \brief Generic scene-graph node for application-defined procedural geometry.
  \ingroup coin_nodes
  \sa SoShape
*/

#include <Inventor/nodes/SoProceduralShape.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <Inventor/SbVec4f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/elements/SoDrawStyleElement.h>
#include <Inventor/elements/SoLineWidthElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/draggers/SoTranslate2Dragger.h>
#include <Inventor/sensors/SoFieldSensor.h>

#include "nodes/SoSubNodeP.h"

// ---------------------------------------------------------------------------
// Minimal JSON parser (self-contained, handles objects/arrays/strings/numbers)
// ---------------------------------------------------------------------------

namespace {

struct JVal;
using JArray  = std::vector<JVal>;
using JObject = std::map<std::string, JVal>;

enum JKind { JK_NULL, JK_BOOL, JK_NUM, JK_STR, JK_ARR, JK_OBJ };

struct JVal {
  JKind       kind = JK_NULL;
  bool        bval = false;
  double      nval = 0.0;
  std::string sval;
  JArray      aval;
  JObject     oval;

  bool   isNull() const { return kind == JK_NULL; }
  bool   isBool() const { return kind == JK_BOOL; }
  bool   isNum()  const { return kind == JK_NUM; }
  bool   isStr()  const { return kind == JK_STR; }
  bool   isArr()  const { return kind == JK_ARR; }
  bool   isObj()  const { return kind == JK_OBJ; }
  bool   getBool()   const { return bval; }
  double getNum()    const { return nval; }
  float  getFloat()  const { return (float)nval; }
  int    getInt()    const { return (int)nval; }
  const std::string& getStr()  const { return sval; }
  const JArray&      getArr()  const { return aval; }
  const JObject&     getObj()  const { return oval; }

  bool contains(const std::string& k) const {
    return kind == JK_OBJ && oval.count(k) > 0;
  }
  const JVal& at(const std::string& k) const {
    static JVal nullv;
    auto it = oval.find(k);
    return it != oval.end() ? it->second : nullv;
  }
};

static void jskip(const char*& p) {
  while (*p && isspace((unsigned char)*p)) ++p;
}
static JVal jparse(const char*& p);

static std::string jstring(const char*& p) {
  ++p; // skip '"'
  std::string s;
  while (*p && *p != '"') {
    if (*p == '\\') {
      ++p;
      switch (*p) {
      case '"': s+='"'; break;  case '\\': s+='\\'; break;
      case '/': s+='/'; break;  case 'n': s+='\n'; break;
      case 't': s+='\t'; break; case 'r': s+='\r'; break;
      case 'b': s+='\b'; break; case 'f': s+='\f'; break;
      default: s+=*p; break;
      }
    } else s += *p;
    ++p;
  }
  if (*p=='"') ++p;
  return s;
}
static JVal jobj(const char*& p) {
  ++p; JVal v; v.kind=JK_OBJ; jskip(p);
  if (*p=='}') { ++p; return v; }
  while (*p) {
    jskip(p); if (*p!='"') break;
    std::string k = jstring(p); jskip(p);
    if (*p==':') { ++p; } jskip(p);
    v.oval[k] = jparse(p); jskip(p);
    if (*p==',') { ++p; jskip(p); }
    else if (*p=='}') { ++p; break; }
    else break;
  }
  return v;
}
static JVal jarr(const char*& p) {
  ++p; JVal v; v.kind=JK_ARR; jskip(p);
  if (*p==']') { ++p; return v; }
  while (*p) {
    jskip(p); v.aval.push_back(jparse(p)); jskip(p);
    if (*p==',') ++p;
    else if (*p==']') { ++p; break; }
    else break;
  }
  return v;
}
static JVal jparse(const char*& p) {
  jskip(p);
  if (!*p) return JVal{};
  if (*p=='{') return jobj(p);
  if (*p=='[') return jarr(p);
  if (*p=='"') { JVal v; v.kind=JK_STR; v.sval=jstring(p); return v; }
  if (strncmp(p,"true",4)==0)  { p+=4; JVal v; v.kind=JK_BOOL; v.bval=true;  return v; }
  if (strncmp(p,"false",5)==0) { p+=5; JVal v; v.kind=JK_BOOL; v.bval=false; return v; }
  if (strncmp(p,"null",4)==0)  { p+=4; return JVal{}; }
  JVal v; v.kind=JK_NUM; char* e=nullptr; v.nval=strtod(p,&e); if(e>p) p=e; return v;
}
static JVal parseJSON(const std::string& s) {
  const char* p = s.c_str(); return jparse(p);
}

// ---------------------------------------------------------------------------
// Parsed topology structures (filled from JSON schema "vertices"/"faces"/"handles")
// ---------------------------------------------------------------------------

struct ParsedParam {
  std::string name, type, label;
  float defVal = 0.f, minVal = 0.f, maxVal = 0.f;
  bool hasMin = false, hasMax = false;
};

// Vertex → three parameter indices (which slot in params[] is its x, y, z)
struct ParsedVertex {
  std::string name;
  int paramX = -1, paramY = -1, paramZ = -1;
};

// Quad face with optional opposite face
struct ParsedFace {
  std::string name;
  std::vector<int> vertexIndices; // into ParsedVertex list
  int oppositeIndex = -1;         // index of opposite ParsedFace (-1 = none)
};

// One entry in the "handles" JSON array
struct ParsedHandle {
  std::string name;
  SoProceduralHandle::DragType dragType = SoProceduralHandle::DRAG_POINT;
  int  elementType = 0;          // 0=vertex, 1=edge, 2=face
  std::vector<int> vertexIdxs;   // vertex/edge: affected vertex list
  int  faceIdx = -1;             // face handles only
};

// ---------------------------------------------------------------------------
// Shape type registry entry
// ---------------------------------------------------------------------------

struct ShapeTypeEntry {
  std::string              typeName;
  std::string              schemaJSON;
  SoProceduralBBoxCB       bboxCB;
  SoProceduralGeomCB       geomCB;
  SoProceduralHandlesCB    handlesCB;       // nullptr when topology drives handles
  SoProceduralHandleDragCB handleDragCB;    // nullptr when topology drives delta
  SoProceduralHandleValidateCB handleValidateCB; // optional per-position clamping
  SoProceduralObjectValidateCB objectValidateCB; // optional object-level JSON check
  void*                    userdata;

  // Parsed topology (populated if JSON includes "vertices"/"faces"/"handles")
  std::vector<ParsedParam>  parsedParams;
  std::vector<ParsedVertex> parsedVertices;
  std::vector<ParsedFace>   parsedFaces;
  std::vector<ParsedHandle> parsedHandles;
  bool topologyParsed = false;
};

// ---------------------------------------------------------------------------
// Registry storage
// ---------------------------------------------------------------------------

std::map<std::string, ShapeTypeEntry> s_registry;

static const ShapeTypeEntry* findEntry(const char* typeName)
{
  if (!typeName || !*typeName) return nullptr;
  auto it = s_registry.find(typeName);
  return (it != s_registry.end()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// JSON topology parser
// ---------------------------------------------------------------------------

static void parseTopology(ShapeTypeEntry& e, const JVal& root)
{
  // --- params section ---
  if (root.contains("params")) {
    for (const JVal& p : root.at("params").getArr()) {
      if (!p.isObj()) continue;
      ParsedParam pp;
      if (p.contains("name")) pp.name = p.at("name").getStr();
      if (p.contains("type")) pp.type = p.at("type").getStr();
      if (p.contains("label")) pp.label = p.at("label").getStr();
      if (p.contains("default")) pp.defVal = p.at("default").getFloat();
      if (p.contains("min"))  { pp.minVal = p.at("min").getFloat();  pp.hasMin = true; }
      if (p.contains("max"))  { pp.maxVal = p.at("max").getFloat();  pp.hasMax = true; }
      e.parsedParams.push_back(pp);
    }
  }

  if (!root.contains("vertices") || !root.contains("faces")) return;

  // --- vertices section ---
  // Build param-name → index map
  std::map<std::string,int> paramIndex;
  for (int i = 0; i < (int)e.parsedParams.size(); ++i)
    paramIndex[e.parsedParams[i].name] = i;

  for (const JVal& v : root.at("vertices").getArr()) {
    if (!v.isObj()) continue;
    ParsedVertex pv;
    if (v.contains("name")) pv.name = v.at("name").getStr();
    auto getIdx = [&](const char* k) -> int {
      if (!v.contains(k)) return -1;
      auto it = paramIndex.find(v.at(k).getStr());
      return it != paramIndex.end() ? it->second : -1;
    };
    pv.paramX = getIdx("x");
    pv.paramY = getIdx("y");
    pv.paramZ = getIdx("z");
    e.parsedVertices.push_back(pv);
  }

  // Build vertex-name → index map
  std::map<std::string,int> vertexIndex;
  for (int i = 0; i < (int)e.parsedVertices.size(); ++i)
    vertexIndex[e.parsedVertices[i].name] = i;

  // --- faces section (first pass: no opposites yet) ---
  std::vector<std::string> faceOppositeNames;
  for (const JVal& f : root.at("faces").getArr()) {
    if (!f.isObj()) continue;
    ParsedFace pf;
    if (f.contains("name")) pf.name = f.at("name").getStr();
    if (f.contains("verts")) {
      for (const JVal& vn : f.at("verts").getArr()) {
        if (!vn.isStr()) continue;
        auto it = vertexIndex.find(vn.getStr());
        pf.vertexIndices.push_back(it != vertexIndex.end() ? it->second : -1);
      }
    }
    faceOppositeNames.push_back(f.contains("opposite") ? f.at("opposite").getStr() : "");
    e.parsedFaces.push_back(pf);
  }

  // Build face-name → index map
  std::map<std::string,int> faceIndex;
  for (int i = 0; i < (int)e.parsedFaces.size(); ++i)
    faceIndex[e.parsedFaces[i].name] = i;

  // Second pass: resolve opposite indices
  for (int i = 0; i < (int)e.parsedFaces.size(); ++i) {
    const std::string& oname = faceOppositeNames[i];
    if (!oname.empty()) {
      auto it = faceIndex.find(oname);
      if (it != faceIndex.end()) e.parsedFaces[i].oppositeIndex = it->second;
    }
  }

  // --- handles section (optional) ---
  if (root.contains("handles")) {
    auto parseDragType = [](const std::string& s) -> SoProceduralHandle::DragType {
      if (s == "DRAG_ALONG_AXIS")    return SoProceduralHandle::DRAG_ALONG_AXIS;
      if (s == "DRAG_ON_PLANE")      return SoProceduralHandle::DRAG_ON_PLANE;
      if (s == "DRAG_NO_INTERSECT")  return SoProceduralHandle::DRAG_NO_INTERSECT;
      return SoProceduralHandle::DRAG_POINT; // default
    };

    for (const JVal& h : root.at("handles").getArr()) {
      if (!h.isObj()) continue;
      ParsedHandle ph;
      if (h.contains("name")) ph.name = h.at("name").getStr();
      std::string dtStr = h.contains("dragType") ? h.at("dragType").getStr() : "";
      ph.dragType = parseDragType(dtStr);

      if (h.contains("vertex")) {
        ph.elementType = 0;
        const std::string& vname = h.at("vertex").getStr();
        auto it = vertexIndex.find(vname);
        if (it != vertexIndex.end()) ph.vertexIdxs.push_back(it->second);
      } else if (h.contains("edge")) {
        ph.elementType = 1;
        for (const JVal& vn : h.at("edge").getArr()) {
          if (!vn.isStr()) continue;
          auto it = vertexIndex.find(vn.getStr());
          if (it != vertexIndex.end()) ph.vertexIdxs.push_back(it->second);
        }
      } else if (h.contains("face")) {
        ph.elementType = 2;
        const std::string& fname = h.at("face").getStr();
        auto it = faceIndex.find(fname);
        if (it != faceIndex.end()) {
          ph.faceIdx = it->second;
          for (int vi : e.parsedFaces[it->second].vertexIndices)
            ph.vertexIdxs.push_back(vi);
        }
      }
      e.parsedHandles.push_back(ph);
    }
  }

  e.topologyParsed = true;
}

// ---------------------------------------------------------------------------
// Get a vertex position from params[] using parsed topology
// ---------------------------------------------------------------------------

static SbVec3f vertexPos(const ShapeTypeEntry& e,
                         int vertIdx,
                         const std::vector<float>& params)
{
  if (vertIdx < 0 || vertIdx >= (int)e.parsedVertices.size())
    return SbVec3f(0,0,0);
  const ParsedVertex& v = e.parsedVertices[vertIdx];
  float x = (v.paramX >= 0 && v.paramX < (int)params.size()) ? params[v.paramX] : 0.f;
  float y = (v.paramY >= 0 && v.paramY < (int)params.size()) ? params[v.paramY] : 0.f;
  float z = (v.paramZ >= 0 && v.paramZ < (int)params.size()) ? params[v.paramZ] : 0.f;
  return SbVec3f(x, y, z);
}

// ---------------------------------------------------------------------------
// Serialise proposed params to JSON for SoProceduralObjectValidateCB
//
// Format:  { "type": "MyShape", "params": { "paramName": 1.23, ... } }
// If no named params are available, index keys "p0", "p1", ... are used.
// ---------------------------------------------------------------------------

static std::string buildProposedParamsJSON(const ShapeTypeEntry&     e,
                                           const std::vector<float>& params)
{
  // Helper: emit a JSON number with enough precision for round-trip float32.
  // "%.9g" uses up to 9 significant digits, which equals FLT_DECIMAL_DIG and
  // guarantees that parsing the output back to float32 yields the original
  // value (IEEE 754 single precision requires exactly 9 decimal digits for
  // this guarantee).
  auto emitNum = [](char* buf, size_t sz, float v) {
    snprintf(buf, sz, "%.9g", (double)v);
    // Append ".0" if no decimal point / exponent so JSON parsers know it is a
    // floating-point value, not an integer.
    bool hasDot = false;
    for (int i = 0; buf[i]; ++i)
      if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') { hasDot = true; break; }
    if (!hasDot) {
      size_t n = strlen(buf);
      if (n + 2 < sz) { buf[n] = '.'; buf[n+1] = '0'; buf[n+2] = '\0'; }
    }
  };

  // Basic JSON string escaping for param names (normally simple identifiers,
  // but we escape backslash and double-quote just in case).
  auto escapeStr = [](const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
      if (c == '"')  { out += '\\'; out += '"';  }
      else if (c == '\\') { out += '\\'; out += '\\'; }
      else out += c;
    }
    out += '"';
    return out;
  };

  std::string json;
  json.reserve(128 + params.size() * 20);
  json += "{\"type\":";
  json += escapeStr(e.typeName);
  json += ",\"params\":{";

  char numBuf[32];
  bool first = true;
  if (!e.parsedParams.empty()) {
    for (int i = 0; i < (int)e.parsedParams.size() && i < (int)params.size(); ++i) {
      if (!first) json += ',';
      json += escapeStr(e.parsedParams[i].name);
      json += ':';
      emitNum(numBuf, sizeof(numBuf), params[i]);
      json += numBuf;
      first = false;
    }
  } else {
    for (int i = 0; i < (int)params.size(); ++i) {
      if (!first) json += ',';
      char kbuf[16];
      snprintf(kbuf, sizeof(kbuf), "\"p%d\":", i);
      json += kbuf;
      emitNum(numBuf, sizeof(numBuf), params[i]);
      json += numBuf;
      first = false;
    }
  }
  json += "}}";
  return json;
}

// ---------------------------------------------------------------------------
// Per-handle drag sensor context (stores sensor ref for dragger reset)
// ---------------------------------------------------------------------------

struct HandleDragContext {
  SoProceduralShape*  shape;
  int                 handleIndex;
  SbVec3f             initPos;         // handle world position at drag start
  std::vector<float>  initParams;      // shape params snapshot at drag start
  SbVec3f             dragStartTrans;  // dragger translation field at drag start
  SoDragger*          dragger;
  SoFieldSensor*      sensor;  // stored so we can detach/reattach on reset
  bool                isNoIntersect; // true → apply validate logic

  HandleDragContext(SoProceduralShape* s, int idx, const SbVec3f& pos,
                    SoDragger* d, bool noIntersect)
    : shape(s), handleIndex(idx), initPos(pos), dragStartTrans(0.f, 0.f, 0.f),
      dragger(d), sensor(nullptr), isNoIntersect(noIntersect) {}
};

// ---------------------------------------------------------------------------
// Drag-start callback: snapshot params and dragger translation so that
// handleDragSensorCB always applies only the *current* drag's delta to a
// known-good baseline instead of accumulating on top of already-updated params.
// ---------------------------------------------------------------------------

static void handleDragStartCB(void* userdata, SoDragger*)
{
  auto* ctx = static_cast<HandleDragContext*>(userdata);
  if (!ctx || !ctx->shape || !ctx->dragger) return;

  // Save current dragger translation (the accumulated offset since the
  // dragger was built) so sensor CB can compute the delta for *this* drag.
  SoField* tf = ctx->dragger->getField("translation");
  if (tf && tf->isOfType(SoSFVec3f::getClassTypeId()))
    ctx->dragStartTrans = static_cast<SoSFVec3f*>(tf)->getValue();
  else
    ctx->dragStartTrans.setValue(0.f, 0.f, 0.f);

  // Snapshot shape params: these are the correct baseline for this drag.
  int nparams = ctx->shape->params.getNum();
  if (nparams > 0) {
    const float* pv = ctx->shape->params.getValues(0);
    ctx->initParams.assign(pv, pv + nparams);
  } else {
    ctx->initParams.clear();
  }

  // Update initPos to the handle's actual world position derived from the
  // current params snapshot.  The constructor's initPos was set when the scene
  // was first built; after one or more earlier drags have moved the handle,
  // this recomputes the correct starting position for the new drag.
  const ShapeTypeEntry* entry =
    findEntry(ctx->shape->shapeType.getValue().getString());
  if (entry && !ctx->initParams.empty() &&
      ctx->handleIndex < (int)entry->parsedHandles.size()) {
    const ParsedHandle& hdl = entry->parsedHandles[ctx->handleIndex];
    SbVec3f pos(0.f, 0.f, 0.f);
    int cnt = 0;
    for (int vi : hdl.vertexIdxs) {
      pos += vertexPos(*entry, vi, ctx->initParams);
      ++cnt;
    }
    if (cnt > 0) pos /= static_cast<float>(cnt);
    ctx->initPos = pos;
  }
}

// ---------------------------------------------------------------------------
// Sensor callback
// ---------------------------------------------------------------------------

static void handleDragSensorCB(void* userdata, SoSensor*)
{
  auto* ctx = static_cast<HandleDragContext*>(userdata);
  if (!ctx || !ctx->shape || !ctx->dragger) return;

  SoProceduralShape*    shape = ctx->shape;
  const ShapeTypeEntry* entry =
    findEntry(shape->shapeType.getValue().getString());
  if (!entry) return;

  // Compute the delta for *this* drag only: subtract the translation that
  // was already accumulated before this drag started.  This prevents the
  // callback from re-applying all previous drags' offsets on every fire.
  SoField* tf = ctx->dragger->getField("translation");
  SbVec3f curTrans(0.f, 0.f, 0.f);
  if (tf && tf->isOfType(SoSFVec3f::getClassTypeId()))
    curTrans = static_cast<SoSFVec3f*>(tf)->getValue();

  SbVec3f delta = curTrans - ctx->dragStartTrans;

  SbVec3f proposedNewPos = ctx->initPos + delta;
  SbVec3f acceptedPos    = proposedNewPos;

  // Use the params snapshot taken at the start of this drag as the baseline.
  // Applying only the current drag's delta to this snapshot ensures the
  // result is correct regardless of how many times the sensor fires.
  int   nparams = shape->params.getNum();
  std::vector<float> paramsCopy;
  if (!ctx->initParams.empty()) {
    paramsCopy = ctx->initParams;
  } else if (nparams > 0) {
    const float* pv = shape->params.getValues(0);
    paramsCopy.assign(pv, pv + nparams);
  }
  paramsCopy.resize(static_cast<size_t>(nparams), 0.f);

  if (ctx->isNoIntersect) {
    // Build proposed params: apply the drag delta to the affected vertices.
    std::vector<float> proposed = paramsCopy;
    if (entry->topologyParsed &&
        ctx->handleIndex < (int)entry->parsedHandles.size()) {
      const ParsedHandle& hdl = entry->parsedHandles[ctx->handleIndex];
      for (int vi : hdl.vertexIdxs) {
        if (vi < 0 || vi >= (int)entry->parsedVertices.size()) continue;
        const ParsedVertex& pv = entry->parsedVertices[vi];
        if (pv.paramX >= 0 && pv.paramX < nparams) proposed[pv.paramX] += delta[0];
        if (pv.paramY >= 0 && pv.paramY < nparams) proposed[pv.paramY] += delta[1];
        if (pv.paramZ >= 0 && pv.paramZ < nparams) proposed[pv.paramZ] += delta[2];
      }
    }

    // 1. Object-level validation: serialise proposed params to JSON and call app.
    if (entry->objectValidateCB) {
      std::string json = buildProposedParamsJSON(*entry, proposed);
      SbBool ok = entry->objectValidateCB(json.c_str(), entry->userdata);
      if (!ok) {
        // Rejected — snap dragger back to its position at the start of this drag.
        acceptedPos = ctx->initPos;
        if (ctx->sensor) ctx->sensor->detach();
        SoSFVec3f* tvf = static_cast<SoSFVec3f*>(tf);
        if (tvf) tvf->setValue(ctx->dragStartTrans);
        if (ctx->sensor && tf) ctx->sensor->attach(tf);
        return; // params unchanged
      }
    }

    // 2. Per-position spatial clamping callback (optional, runs after object CB).
    if (entry->handleValidateCB) {
      SbBool cbOK = entry->handleValidateCB(
          paramsCopy.data(), nparams,
          ctx->handleIndex, ctx->initPos, proposedNewPos,
          acceptedPos, entry->userdata);
      (void)cbOK;
    }

    // If accepted position differs from proposed, reset the dragger field.
    // 1 micrometre squared threshold avoids spurious resets from float noise.
    static const float kDraggerResetThreshSq = 1e-6f * 1e-6f;
    if ((acceptedPos - proposedNewPos).sqrLength() > kDraggerResetThreshSq) {
      // Compose new translation: drag-start offset + accepted delta for this drag.
      SbVec3f newTrans = ctx->dragStartTrans + (acceptedPos - ctx->initPos);
      if (ctx->sensor) ctx->sensor->detach();
      SoSFVec3f* tvf = static_cast<SoSFVec3f*>(tf);
      if (tvf) tvf->setValue(newTrans);
      if (ctx->sensor && tf) ctx->sensor->attach(tf);
    }
  }

  // --- Apply drag via topology-based built-in OR application callback ---
  if (entry->topologyParsed && !entry->handleDragCB &&
      ctx->handleIndex < (int)entry->parsedHandles.size()) {
    // Built-in: apply accepted delta to initParams baseline
    const ParsedHandle& hdl = entry->parsedHandles[ctx->handleIndex];

    SbVec3f appliedDelta = acceptedPos - ctx->initPos;
    for (int vi : hdl.vertexIdxs) {
      if (vi < 0 || vi >= (int)entry->parsedVertices.size()) continue;
      const ParsedVertex& pv = entry->parsedVertices[vi];
      if (pv.paramX >= 0 && pv.paramX < nparams) paramsCopy[pv.paramX] += appliedDelta[0];
      if (pv.paramY >= 0 && pv.paramY < nparams) paramsCopy[pv.paramY] += appliedDelta[1];
      if (pv.paramZ >= 0 && pv.paramZ < nparams) paramsCopy[pv.paramZ] += appliedDelta[2];
    }
    shape->params.setValues(0, nparams, paramsCopy.data());

  } else if (entry->handleDragCB) {
    // Application-supplied callback: receives initParams baseline + drag positions
    entry->handleDragCB(paramsCopy.data(), nparams,
                        ctx->handleIndex,
                        ctx->initPos, acceptedPos,
                        entry->userdata);
    shape->params.setValues(0, nparams, paramsCopy.data());
  }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SO_NODE macros
// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoProceduralShape);

// ---------------------------------------------------------------------------
// initClass / constructor / destructor
// ---------------------------------------------------------------------------

void
SoProceduralShape::initClass()
{
  SO_NODE_INIT_CLASS(SoProceduralShape, SoShape, "SoShape");
}

SoProceduralShape::SoProceduralShape()
{
  SO_NODE_CONSTRUCTOR(SoProceduralShape);
  SO_NODE_ADD_FIELD(shapeType,  (""));
  SO_NODE_ADD_FIELD(schemaJSON, (""));
  SO_NODE_ADD_FIELD(params,     (0.0f));
  this->params.setNum(0);
  this->d_selectCB         = nullptr;
  this->d_useCustomSelectCB = FALSE;
  this->d_selectUserdata   = nullptr;
}

SoProceduralShape::~SoProceduralShape()
{
}

// ---------------------------------------------------------------------------
// Registry helpers
// ---------------------------------------------------------------------------

// static
SbBool
SoProceduralShape::registerShapeType(const char*        typeName,
                                     const char*        schemaJSON,
                                     SoProceduralBBoxCB bboxCB,
                                     SoProceduralGeomCB geomCB,
                                     void*              userdata)
{
  return SoProceduralShape::registerShapeType(typeName, schemaJSON,
                                              bboxCB, geomCB,
                                              nullptr, nullptr,
                                              nullptr, nullptr,
                                              userdata);
}

// static
SbBool
SoProceduralShape::registerShapeType(const char*              typeName,
                                     const char*              schemaJSON,
                                     SoProceduralBBoxCB       bboxCB,
                                     SoProceduralGeomCB       geomCB,
                                     SoProceduralHandlesCB    handlesCB,
                                     SoProceduralHandleDragCB handleDragCB,
                                     void*                    userdata)
{
  return SoProceduralShape::registerShapeType(typeName, schemaJSON,
                                              bboxCB, geomCB,
                                              handlesCB, handleDragCB,
                                              nullptr, nullptr,
                                              userdata);
}

// static
SbBool
SoProceduralShape::registerShapeType(const char*                  typeName,
                                     const char*                  schemaJSON,
                                     SoProceduralBBoxCB           bboxCB,
                                     SoProceduralGeomCB           geomCB,
                                     SoProceduralHandlesCB        handlesCB,
                                     SoProceduralHandleDragCB     handleDragCB,
                                     SoProceduralHandleValidateCB handleValidateCB,
                                     SoProceduralObjectValidateCB objectValidateCB,
                                     void*                        userdata)
{
  if (!typeName || !*typeName || !bboxCB || !geomCB) return FALSE;
  if (s_registry.find(typeName) != s_registry.end()) return FALSE;

  ShapeTypeEntry entry;
  entry.typeName        = typeName;
  entry.schemaJSON      = schemaJSON ? schemaJSON : "";
  entry.bboxCB          = bboxCB;
  entry.geomCB          = geomCB;
  entry.handlesCB       = handlesCB;
  entry.handleDragCB    = handleDragCB;
  entry.handleValidateCB = handleValidateCB;
  entry.objectValidateCB = objectValidateCB;
  entry.userdata        = userdata;

  // Parse the JSON schema to extract params and optional topology
  if (!entry.schemaJSON.empty()) {
    JVal root = parseJSON(entry.schemaJSON);
    if (root.isObj()) parseTopology(entry, root);
  }

  s_registry[typeName] = std::move(entry);
  return TRUE;
}

// static
SbBool
SoProceduralShape::setObjectValidateCallback(const char*                  typeName,
                                             SoProceduralObjectValidateCB objectValidateCB,
                                             void*                        userdata)
{
  if (!typeName || !*typeName) return FALSE;
  auto it = s_registry.find(typeName);
  if (it == s_registry.end()) return FALSE;
  it->second.objectValidateCB = objectValidateCB;
  it->second.userdata         = userdata;  // always updated (pass nullptr to clear)
  return TRUE;
}

// static
const char*
SoProceduralShape::getSchemaJSON(const char* typeName)
{
  auto it = s_registry.find(typeName);
  if (it == s_registry.end()) return nullptr;
  return it->second.schemaJSON.c_str();
}

// static
SbBool
SoProceduralShape::isRegistered(const char* typeName)
{
  return s_registry.find(typeName) != s_registry.end() ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Convenience: setShapeType
// ---------------------------------------------------------------------------

/*!
  Set the \c shapeType field to \a typeName, copy the registered JSON schema
  into \c schemaJSON, and populate \c params with the default values extracted
  from the schema's "default" entries (in declaration order).

  If \a typeName is not registered the fields are still set but \c params is
  left empty.
*/
void
SoProceduralShape::setShapeType(const char* typeName)
{
  this->shapeType.setValue(typeName ? typeName : "");

  const ShapeTypeEntry* entry = findEntry(typeName);
  if (!entry) return;

  this->schemaJSON.setValue(entry->schemaJSON.c_str());

  // Use parsed params (populated during registerShapeType) when available.
  if (!entry->parsedParams.empty()) {
    std::vector<float> defaults;
    defaults.reserve(entry->parsedParams.size());
    for (const ParsedParam& pp : entry->parsedParams)
      defaults.push_back(pp.defVal);
    this->params.setNum(0);
    this->params.setValues(0, static_cast<int>(defaults.size()), defaults.data());
    return;
  }

  // Fallback: simple linear scan for "default": <number> entries.
  const char* schema = entry->schemaJSON.c_str();
  std::vector<float> defaults;
  const char* p = schema;
  while ((p = strstr(p, "\"default\"")) != nullptr) {
    p += 9;
    while (*p && (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      ++p;
    if (!*p) break;
    char* end = nullptr;
    float v = strtof(p, &end);
    if (end && end > p) defaults.push_back(v);
    p = end ? end : p + 1;
  }
  if (!defaults.empty()) {
    this->params.setNum(0);
    this->params.setValues(0, static_cast<int>(defaults.size()), defaults.data());
  }
}

// ---------------------------------------------------------------------------
// buildHandleDraggers
// ---------------------------------------------------------------------------

/*!
  Build and return a separator containing one dragger per registered handle.

  Handles are sourced from one of two places (in priority order):
  1. Application \c handlesCB callback (if non-null).
  2. The "handles" section of the JSON schema, when topology has been parsed
     (no \c handlesCB or \c handleDragCB needed in that case).

  Each dragger is placed at the handle position via an SoTranslation node
  and wired to the shape's \c params field through an SoFieldSensor.

  Dragger selection by DragType:
  - DRAG_POINT / DRAG_NO_INTERSECT → SoDragPointDragger
  - DRAG_ALONG_AXIS → SoTranslate1Dragger (axis aligned via SoRotation)
  - DRAG_ON_PLANE   → SoTranslate2Dragger (plane via SoRotation)

  For DRAG_NO_INTERSECT: if topology is parsed, the built-in face-flip test
  runs before each delta is applied; rejected moves snap the dragger back.
  The optional SoProceduralHandleValidateCB runs after the built-in test.

  Returns nullptr when no handles can be derived.
*/
SoSeparator*
SoProceduralShape::buildHandleDraggers()
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return nullptr;

  int          nparams = this->params.getNum();
  const float* pv      = nparams > 0 ? this->params.getValues(0) : nullptr;
  std::vector<float> pvVec(pv, pv + nparams);

  // --- Gather handle descriptors ---
  std::vector<SoProceduralHandle> handles;

  if (entry->handlesCB) {
    // Application-supplied callback
    entry->handlesCB(pv, nparams, handles, entry->userdata);
  } else if (entry->topologyParsed && !entry->parsedHandles.empty()) {
    // Auto-generate from parsed topology
    for (int i = 0; i < (int)entry->parsedHandles.size(); ++i) {
      const ParsedHandle& ph = entry->parsedHandles[i];
      SoProceduralHandle h;
      h.name     = SbString(ph.name.c_str());
      h.dragType = ph.dragType;

      // Compute position from affected vertices
      SbVec3f pos(0,0,0);
      int cnt = 0;
      for (int vi : ph.vertexIdxs) {
        pos += vertexPos(*entry, vi, pvVec);
        ++cnt;
      }
      if (cnt > 0) pos /= (float)cnt;
      h.position = pos;

      // For DRAG_ON_PLANE (edge): use first adjacent face's normal as plane normal
      if (ph.dragType == SoProceduralHandle::DRAG_ON_PLANE &&
          ph.elementType == 1 && ph.vertexIdxs.size() == 2) {
        // Find a face containing both vertices
        for (const ParsedFace& face : entry->parsedFaces) {
          bool hasA = false, hasB = false;
          for (int vi : face.vertexIndices) {
            if (vi == ph.vertexIdxs[0]) hasA = true;
            if (vi == ph.vertexIdxs[1]) hasB = true;
          }
          if (hasA && hasB && face.vertexIndices.size() >= 3) {
            SbVec3f v0 = vertexPos(*entry, face.vertexIndices[0], pvVec);
            SbVec3f v1 = vertexPos(*entry, face.vertexIndices[1], pvVec);
            SbVec3f v2 = vertexPos(*entry, face.vertexIndices[2], pvVec);
            h.normal = (v1-v0).cross(v2-v0);
            h.normal.normalize();
            break;
          }
        }
      }

      // For DRAG_ALONG_AXIS (face): use face normal as axis
      if (ph.dragType == SoProceduralHandle::DRAG_ALONG_AXIS &&
          ph.elementType == 2 && ph.faceIdx >= 0 &&
          ph.faceIdx < (int)entry->parsedFaces.size()) {
        const ParsedFace& face = entry->parsedFaces[ph.faceIdx];
        if (face.vertexIndices.size() >= 3) {
          SbVec3f v0 = vertexPos(*entry, face.vertexIndices[0], pvVec);
          SbVec3f v1 = vertexPos(*entry, face.vertexIndices[1], pvVec);
          SbVec3f v2 = vertexPos(*entry, face.vertexIndices[2], pvVec);
          h.axis = (v1-v0).cross(v2-v0);
          h.axis.normalize();
        }
      }

      handles.push_back(h);
    }
  }

  if (handles.empty()) return nullptr;

  // Compute a scale factor for dragger widget geometry proportional to the
  // shape's bounding-box extent.  Smaller shapes get smaller handle widgets
  // so the controls don't dwarf the geometry.  The scale is applied via the
  // dragger's initial motionMatrix, which affects only the visual size; the
  // drag projections and reported translation deltas remain in parent-space
  // (world) coordinates and are unaffected.
  float handleScale = 1.0f;
  if (entry->bboxCB && nparams > 0) {
    SbVec3f mn, mx;
    entry->bboxCB(pv, nparams, mn, mx, entry->userdata);
    SbVec3f sz = mx - mn;
    float maxDim = sz[0];
    if (sz[1] > maxDim) maxDim = sz[1];
    if (sz[2] > maxDim) maxDim = sz[2];
    if (maxDim > 1e-6f) {
      // Handle widget size = 20% of the largest bounding-box dimension,
      // clamped to [0.04, 2.0] to avoid degenerate sizes.
      handleScale = maxDim * 0.20f;
      if (handleScale < 0.04f) handleScale = 0.04f;
      if (handleScale > 2.0f)  handleScale = 2.0f;
    }
  }

  SoSeparator* sep = new SoSeparator;

  for (int i = 0; i < (int)handles.size(); ++i) {
    const SoProceduralHandle& h = handles[i];

    SoSeparator* hsep = new SoSeparator;

    SoTranslation* trans = new SoTranslation;
    trans->translation.setValue(h.position);
    hsep->addChild(trans);

    SoDragger* dragger = nullptr;

    switch (h.dragType) {
    case SoProceduralHandle::DRAG_ALONG_AXIS: {
      SbVec3f normAxis = h.axis;
      if (normAxis.normalize() > 1e-6f) {
        SbRotation rot(SbVec3f(1.f, 0.f, 0.f), normAxis);
        SoRotation* rotNode = new SoRotation;
        rotNode->rotation.setValue(rot);
        hsep->addChild(rotNode);
      }
      dragger = new SoTranslate1Dragger;
      break;
    }
    case SoProceduralHandle::DRAG_ON_PLANE: {
      SbVec3f normNml = h.normal;
      if (normNml.normalize() > 1e-6f) {
        SbRotation rot(SbVec3f(0.f, 1.f, 0.f), normNml);
        SoRotation* rotNode = new SoRotation;
        rotNode->rotation.setValue(rot);
        hsep->addChild(rotNode);
      }
      dragger = new SoTranslate2Dragger;
      break;
    }
    case SoProceduralHandle::DRAG_NO_INTERSECT:
    case SoProceduralHandle::DRAG_POINT:
    default:
      dragger = new SoDragPointDragger;
      break;
    }

    // Scale the dragger widget to match the shape's bounding-box extent.
    // Using the initial motionMatrix (not a parent SoScale) keeps the
    // drag delta reported in parent-coordinate (world) space.
    if (handleScale != 1.0f) {
      SbMatrix scaleMtx;
      scaleMtx.setScale(SbVec3f(handleScale, handleScale, handleScale));
      dragger->setMotionMatrix(scaleMtx);
    }

    hsep->addChild(dragger);
    sep->addChild(hsep);

    bool isNoIntersect = (h.dragType == SoProceduralHandle::DRAG_NO_INTERSECT);
    auto* ctx = new HandleDragContext(this, i, h.position, dragger, isNoIntersect);

    // Register start callback to snapshot params and translation at the
    // beginning of each drag so the sensor CB applies only the current
    // drag's delta to a known-good baseline.
    dragger->addStartCallback(handleDragStartCB, ctx);

    SoField* tf = dragger->getField("translation");
    if (tf) {
      SoFieldSensor* sensor = new SoFieldSensor(handleDragSensorCB, ctx);
      ctx->sensor = sensor;  // store for possible reset
      sensor->attach(tf);
      (void)sensor;
    }
  }

  return sep;
}

// ---------------------------------------------------------------------------
// buildSelectionDisplay
// ---------------------------------------------------------------------------

/*!
  Build a scene separator containing one visual handle marker per defined
  handle.  Each marker sub-separator contains:
    - SoTranslation  — places it at the handle position in object space.
    - SoSphere       — visual hit target; radius 0.05 (vertex), 0.07 (edge),
                       0.10 (face).
    - SoText2        — label from the handle's "name" field in the JSON schema.

  The handle set is derived from the same sources as buildHandleDraggers():
  the application's SoProceduralHandlesCB (if registered) or the parsed
  "handles" topology from the JSON schema.

  Returns nullptr when no handles can be derived.
*/
SoSeparator*
SoProceduralShape::buildSelectionDisplay() const
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return nullptr;

  int          nparams = this->params.getNum();
  const float* pv      = nparams > 0 ? this->params.getValues(0) : nullptr;
  std::vector<float> pvVec(pv, pv + nparams);

  // Gather handle descriptors (same logic as buildHandleDraggers)
  std::vector<SoProceduralHandle> handles;

  if (entry->handlesCB) {
    entry->handlesCB(pv, nparams, handles, entry->userdata);
  } else if (entry->topologyParsed && !entry->parsedHandles.empty()) {
    for (int i = 0; i < (int)entry->parsedHandles.size(); ++i) {
      const ParsedHandle& ph = entry->parsedHandles[i];
      SoProceduralHandle h;
      h.name     = SbString(ph.name.c_str());
      h.dragType = ph.dragType;

      SbVec3f pos(0, 0, 0);
      int cnt = 0;
      for (int vi : ph.vertexIdxs) {
        pos += vertexPos(*entry, vi, pvVec);
        ++cnt;
      }
      if (cnt > 0) pos /= (float)cnt;
      h.position = pos;
      handles.push_back(h);
    }
  }

  if (handles.empty()) return nullptr;

  SoSeparator* sep = new SoSeparator;

  for (int i = 0; i < (int)handles.size(); ++i) {
    const SoProceduralHandle& h = handles[i];
    const ParsedHandle*       ph =
      (entry->topologyParsed && i < (int)entry->parsedHandles.size())
        ? &entry->parsedHandles[i] : nullptr;

    // Radius: vertex → 0.12, edge → 0.09, face → 0.18
    float radius = 0.12f;
    if (ph) {
      if (ph->elementType == 1) radius = 0.09f;  // edge
      if (ph->elementType == 2) radius = 0.18f;  // face
    }

    SoSeparator* hsep = new SoSeparator;

    SoTranslation* trans = new SoTranslation;
    trans->translation.setValue(h.position);
    hsep->addChild(trans);

    SoSphere* sphere = new SoSphere;
    sphere->radius.setValue(radius);
    hsep->addChild(sphere);

    SoFont* font = new SoFont;
    font->size.setValue(12.0f);
    hsep->addChild(font);

    // depthTest=FALSE: label always renders on top of the sphere and solid
    // geometry regardless of depth — mirrors SGI OpenInventor 2.1 behaviour.
    SoText2* label = new SoText2;
    label->string.setValue(h.name);
    label->depthTest.setValue(FALSE);
    hsep->addChild(label);

    sep->addChild(hsep);
  }

  return sep;
}

// ---------------------------------------------------------------------------
// setSelectCallback / pickHandle
// ---------------------------------------------------------------------------

void
SoProceduralShape::setSelectCallback(SoProceduralSelectCB selectCB,
                                     SbBool               useCustomCB,
                                     void*                userdata)
{
  this->d_selectCB          = selectCB;
  this->d_useCustomSelectCB = useCustomCB;
  this->d_selectUserdata    = userdata;
}

/*!
  Return the 0-based index of the handle hit by the given ray.

  Built-in test: for each handle, computes the squared distance from the
  closest point on the ray to the handle position.  A hit is registered when
  the distance is below \a proximityThreshSq.

  The application callback (if set) is used instead of, or as a fallback
  for, the built-in test depending on the \c useCustomCB flag.
*/
int
SoProceduralShape::pickHandle(const SbVec3f& rayOrigin,
                               const SbVec3f& rayDir,
                               float          proximityThreshSq) const
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return -1;

  int          nparams = this->params.getNum();
  const float* pv      = nparams > 0 ? this->params.getValues(0) : nullptr;
  std::vector<float> pvVec(pv, pv + nparams);

  // Gather handle positions (same logic as buildSelectionDisplay)
  std::vector<SoProceduralHandle> handles;
  if (entry->handlesCB) {
    entry->handlesCB(pv, nparams, handles, entry->userdata);
  } else if (entry->topologyParsed && !entry->parsedHandles.empty()) {
    for (int i = 0; i < (int)entry->parsedHandles.size(); ++i) {
      const ParsedHandle& ph = entry->parsedHandles[i];
      SoProceduralHandle h;
      h.name     = SbString(ph.name.c_str());
      h.dragType = ph.dragType;
      SbVec3f pos(0, 0, 0);
      int cnt = 0;
      for (int vi : ph.vertexIdxs) {
        pos += vertexPos(*entry, vi, pvVec);
        ++cnt;
      }
      if (cnt > 0) pos /= (float)cnt;
      h.position = pos;
      handles.push_back(h);
    }
  }

  int numHandles = (int)handles.size();

  // If custom CB takes full responsibility, skip built-in test entirely.
  if (this->d_useCustomSelectCB && this->d_selectCB)
    return this->d_selectCB(rayOrigin, rayDir, numHandles, this->d_selectUserdata);

  // Built-in vertex-proximity test.
  // Closest point on ray to handle position: project (pos - origin) onto dir.
  SbVec3f dirNorm = rayDir;
  float   dirLen  = dirNorm.normalize();
  if (dirLen < 1e-10f) return -1;  // degenerate ray — no hit possible

  int   bestIdx   = -1;
  float bestDistSq = proximityThreshSq;

  for (int i = 0; i < numHandles; ++i) {
    SbVec3f toPos = handles[i].position - rayOrigin;
    float   t     = toPos.dot(dirNorm);
    SbVec3f closest = rayOrigin + dirNorm * t;
    float   dSq   = (handles[i].position - closest).sqrLength();
    if (dSq < bestDistSq) {
      bestDistSq = dSq;
      bestIdx    = i;
    }
  }

  // Fallback to app callback when built-in missed.
  if (bestIdx == -1 && this->d_selectCB)
    bestIdx = this->d_selectCB(rayOrigin, rayDir, numHandles, this->d_selectUserdata);

  return bestIdx;
}

// ---------------------------------------------------------------------------
// getCurrentParamsJSON
// ---------------------------------------------------------------------------

/*!
  Serialise the shape's current \c params field to a JSON string using the
  same format as SoProceduralObjectValidateCB:
  \code
  { "type": "MyShape", "params": { "paramName": 1.23, ... } }
  \endcode
*/
SbString
SoProceduralShape::getCurrentParamsJSON() const
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return SbString("");

  int          nparams = this->params.getNum();
  const float* pv      = nparams > 0 ? this->params.getValues(0) : nullptr;
  std::vector<float> pvVec(pv, pv + nparams);

  std::string json = buildProposedParamsJSON(*entry, pvVec);
  return SbString(json.c_str());
}

// ---------------------------------------------------------------------------
// computeBBox
// ---------------------------------------------------------------------------

void
SoProceduralShape::computeBBox(SoAction* /* action */, SbBox3f& box, SbVec3f& center)
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());

  if (!entry) {
    // Unknown type: return a unit bounding box so the scene graph stays valid.
    box.setBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    center.setValue(0.0f, 0.0f, 0.0f);
    return;
  }

  int   nparams     = this->params.getNum();
  const float* pv   = nparams > 0 ? this->params.getValues(0) : nullptr;

  SbVec3f minPt(-1.0f, -1.0f, -1.0f);
  SbVec3f maxPt( 1.0f,  1.0f,  1.0f);

  entry->bboxCB(pv, nparams, minPt, maxPt, entry->userdata);

  box.setBounds(minPt, maxPt);
  center = (minPt + maxPt) * 0.5f;
}

// ---------------------------------------------------------------------------
// generatePrimitives — used by all actions: GL render, picking, callback, etc.
//
// This is called by the base class SoShape::GLRender() after it sets up
// the material bundle and internal render state. For GLRenderAction the
// draw style is consulted so that wireframe geometry is emitted when LINES
// is active; for all other actions (ray-pick, callback, prim count) the
// triangle mesh is used.
// ---------------------------------------------------------------------------

void
SoProceduralShape::generatePrimitives(SoAction* action)
{
  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return;

  int         nparams = this->params.getNum();
  const float* pv     = nparams > 0 ? this->params.getValues(0) : nullptr;

  // Consult the current draw style for all action types that carry state.
  // (For GLRenderAction the element is always present; for SoCallbackAction
  // and SoRaytraceRenderAction it is set when a SoDrawStyle node is traversed.)
  bool wireframeMode = false;
  SoState* state = action->getState();
  if (state) {
    wireframeMode = (SoDrawStyleElement::get(state) == SoDrawStyleElement::LINES);
  }

  if (wireframeMode) {
    SoProceduralWireframe wire;
    entry->geomCB(pv, nparams, nullptr, &wire, entry->userdata);
    if (action->getTypeId().isDerivedFrom(SoGLRenderAction::getClassTypeId())) {
      // GL path: emit line primitives as usual.
      emitWireframe(action, wire);
    } else {
      // Non-GL path (e.g. SoCallbackAction for nanort): render each wireframe
      // segment as a thin tessellated cylinder so that triangle callbacks pick
      // it up correctly.  Compute the world-space radius from the current line
      // width and view volume when available, otherwise fall back to a small
      // fixed value.
      float radius = 0.02f;
      if (state) {
        float lineW = SoLineWidthElement::get(state);
        if (lineW <= 0.0f) lineW = 1.0f;
        const SbViewVolume& vv = SoViewVolumeElement::get(state);
        float wh = vv.getHeight();
        if (wh > 1e-6f) {
          // For perspective cameras vv.getHeight() is the frustum height at
          // the near plane.  Scale to the object's viewing distance so that
          // the cylinder radius corresponds to lineW pixels at object depth.
          if (vv.getProjectionType() == SbViewVolume::PERSPECTIVE) {
            const float nearDist = vv.getNearDist();
            if (nearDist > 1e-6f) {
              const SbMatrix& mm = SoModelMatrixElement::get(state);
              const SbVec3f objPos(mm[3][0], mm[3][1], mm[3][2]);
              const float dist =
                (objPos - vv.getProjectionPoint()).length();
              const float refDist = (dist > nearDist) ? dist : nearDist;
              wh = wh * refDist / nearDist;
            }
          }
          const SbViewportRegion& vpr = SoViewportRegionElement::get(state);
          float vpH = static_cast<float>(vpr.getViewportSizePixels()[1]);
          if (vpH > 0.0f)
            radius = lineW * wh / vpH * 0.5f;
        }
      }
      emitWireframeCylinders(action, wire, radius);
    }
  } else {
    SoProceduralTriangles tris;
    entry->geomCB(pv, nparams, &tris, nullptr, entry->userdata);
    emitTriangles(action, tris);
  }
}

// ---------------------------------------------------------------------------
// getPrimitiveCount
// ---------------------------------------------------------------------------

void
SoProceduralShape::getPrimitiveCount(SoGetPrimitiveCountAction* action)
{
  if (!this->shouldPrimitiveCount(action)) return;

  const ShapeTypeEntry* entry =
    findEntry(this->shapeType.getValue().getString());
  if (!entry) return;

  int         nparams = this->params.getNum();
  const float* pv     = nparams > 0 ? this->params.getValues(0) : nullptr;

  SoProceduralTriangles tris;
  entry->geomCB(pv, nparams, &tris, nullptr, entry->userdata);

  int ntris = static_cast<int>(tris.indices.size()) / 3;
  for (int i = 0; i < ntris; ++i)
    action->incNumTriangles();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void
SoProceduralShape::emitTriangles(SoAction*                    action,
                                 const SoProceduralTriangles& tris)
{
  if (tris.vertices.empty() || tris.indices.empty()) return;

  const int  nv          = static_cast<int>(tris.vertices.size());
  const bool hasNormals  = (static_cast<int>(tris.normals.size()) == nv);
  const SbVec3f defNorm(0.0f, 0.0f, 1.0f);

  SoPrimitiveVertex pv;

  this->beginShape(action, SoShape::TRIANGLES);
  for (size_t i = 0; i + 2 < tris.indices.size(); i += 3) {
    const int32_t triIdx[3] = {
      tris.indices[i], tris.indices[i + 1], tris.indices[i + 2]
    };
    // Skip the whole triangle if any index is out of range so that
    // beginShape(TRIANGLES) vertex groups remain properly aligned.
    if (triIdx[0] < 0 || triIdx[0] >= nv ||
        triIdx[1] < 0 || triIdx[1] >= nv ||
        triIdx[2] < 0 || triIdx[2] >= nv)
      continue;

    for (int j = 0; j < 3; ++j) {
      const int32_t idx = triIdx[j];
      pv.setPoint(tris.vertices[static_cast<size_t>(idx)]);
      pv.setNormal(hasNormals ? tris.normals[static_cast<size_t>(idx)]
                              : defNorm);
      pv.setTextureCoords(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f));
      this->shapeVertex(&pv);
    }
  }
  this->endShape();
}

void
SoProceduralShape::emitWireframe(SoAction*                    action,
                                 const SoProceduralWireframe& wire)
{
  if (wire.vertices.empty() || wire.segments.empty()) return;

  const int     nv       = static_cast<int>(wire.vertices.size());
  const SbVec3f defNorm(0.0f, 0.0f, 1.0f);

  SoPrimitiveVertex pv;
  pv.setNormal(defNorm);
  pv.setTextureCoords(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f));

  this->beginShape(action, SoShape::LINES);
  for (size_t i = 0; i + 1 < wire.segments.size(); i += 2) {
    int32_t a = wire.segments[i];
    int32_t b = wire.segments[i + 1];
    if (a < 0 || b < 0) continue;
    if (a >= nv || b >= nv) continue;

    pv.setPoint(wire.vertices[static_cast<size_t>(a)]);
    this->shapeVertex(&pv);
    pv.setPoint(wire.vertices[static_cast<size_t>(b)]);
    this->shapeVertex(&pv);
  }
  this->endShape();
}

void
SoProceduralShape::emitWireframeCylinders(SoAction*                    action,
                                          const SoProceduralWireframe& wire,
                                          float                        radius)
{
  // Tessellate each wireframe segment as a 6-sided prism (cylinder) so that
  // ray-tracing backends (e.g. nanort via SoCallbackAction) can pick up the
  // geometry via their triangle callbacks.  This mirrors the cylinder-proxy
  // mechanism used for SoLineSet / SoIndexedLineSet in nanort_context_manager.h.
  if (wire.vertices.empty() || wire.segments.empty()) return;

  const int    nv     = static_cast<int>(wire.vertices.size());
  // 6-sided (hexagonal) cross-section: efficient for thin wires and still
  // visually round enough for the wireframe-as-cylinders approximation.
  const int    kSides = 6;
  const float  twoPi  = 2.0f * static_cast<float>(M_PI);

  SoPrimitiveVertex pv;
  pv.setTextureCoords(SbVec4f(0.0f, 0.0f, 0.0f, 1.0f));

  this->beginShape(action, SoShape::TRIANGLES);

  for (size_t si = 0; si + 1 < wire.segments.size(); si += 2) {
    const int32_t ia = wire.segments[si];
    const int32_t ib = wire.segments[si + 1];
    if (ia < 0 || ib < 0 || ia >= nv || ib >= nv) continue;

    const SbVec3f& A = wire.vertices[static_cast<size_t>(ia)];
    const SbVec3f& B = wire.vertices[static_cast<size_t>(ib)];

    SbVec3f axis = B - A;
    const float axisLen = axis.length();
    if (axisLen < 1e-8f) continue;
    axis /= axisLen;

    // Build an orthonormal basis {u, v} perpendicular to axis.
    // 0.9f ≈ cos(26°): switch reference vector when axis is nearly parallel
    // to Y (within 26°) to avoid a degenerate cross-product.
    SbVec3f ref(0.0f, 1.0f, 0.0f);
    if (fabsf(axis.dot(ref)) > 0.9f)
      ref = SbVec3f(1.0f, 0.0f, 0.0f);
    SbVec3f u = ref.cross(axis);  u.normalize();
    SbVec3f v = axis.cross(u);    // already unit length

    // Precompute ring positions and outward normals for both ends.
    SbVec3f posA[kSides], posB[kSides], norm[kSides];
    for (int k = 0; k < kSides; ++k) {
      const float angle = twoPi * k / kSides;
      const SbVec3f n   = u * cosf(angle) + v * sinf(angle);
      posA[k] = A + n * radius;
      posB[k] = B + n * radius;
      norm[k] = n;
    }

    // Emit two triangles per prism face (no end-caps needed for thin wires).
    for (int k = 0; k < kSides; ++k) {
      const int kn = (k + 1) % kSides;

      // Triangle 1: A[k] → A[kn] → B[k]
      pv.setNormal(norm[k]);  pv.setPoint(posA[k]);  this->shapeVertex(&pv);
      pv.setNormal(norm[kn]); pv.setPoint(posA[kn]); this->shapeVertex(&pv);
      pv.setNormal(norm[k]);  pv.setPoint(posB[k]);  this->shapeVertex(&pv);

      // Triangle 2: A[kn] → B[kn] → B[k]
      pv.setNormal(norm[kn]); pv.setPoint(posA[kn]); this->shapeVertex(&pv);
      pv.setNormal(norm[kn]); pv.setPoint(posB[kn]); this->shapeVertex(&pv);
      pv.setNormal(norm[k]);  pv.setPoint(posB[k]);  this->shapeVertex(&pv);
    }
  }

  this->endShape();
}
