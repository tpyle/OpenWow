#pragma once

#include <cstdint>

namespace openwow::ui::game {

int ParseTooltipTexCoords(int start_index, void* lua_state, float* out_coords);

const char* GetColorblindStatSuffix(int stat_index);

int RegisterTooltipScriptHandler(void* tooltip, const char* event_name,
                                 void* handler_info);

void LoadTooltipXMLAttributes(void* tooltip, void* xml_node, void* arg3,
                              void* arg4, int arg5);

const void* GetItemQualityColorPtr(uint32_t quality);

const char* GetItemQualityHexColor(uint32_t quality);

const char* BuildTalentLink(const uint32_t* talent_id, const char* talent_name,
                            int talent_rank);

void SetItemSetThresholdTableForSort(const uint32_t* thresholds);
int CompareItemSetSpellThreshold(const void* lhs, const void* rhs);

int FrameStackInfo_Compare(const void* lhs, const void* rhs);

char* FormatMultiUnitDurationText(char* output, uint32_t output_size,
                                  uint64_t duration_ms,
                                  const char* wrapper_format_key);

void FormatDurationText(char* output, uint32_t output_size, float duration_ms,
                        const char* prefix_key, int count_param);

int FireOnTooltipSetDefaultAnchor(int* tooltip);

void UpdateTooltipLayout(void* tooltip, bool force_update);

}
