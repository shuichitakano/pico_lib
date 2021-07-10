/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 13:12:53
 */
#ifndef _64C895CE_5134_63A8_C843_0FADACBE65CD
#define _64C895CE_5134_63A8_C843_0FADACBE65CD

#include "timing.h"
#include "defines.h"
#include "data_packet.h"
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <stdint.h>
#include <array>
#include <utility>

namespace dvi
{
    class DMA
    {
    public:
        DMA(const Timing &timing, PIO pio);

        void start();
        void __not_in_flash_func(clearInterruptReq)() const;
        void __not_in_flash_func(waitForLastBlockTransferToStart)(const Timing &timing) const;
        void __not_in_flash_func(update)(LineState st, const uint32_t *tmdsBuf, const Timing &timing);

        void setupInternalDataPacketStream();
        void __not_in_flash_func(updateNextDataPacket)(LineState st, const DataPacket &packet, const Timing &timing);

    private:
        struct Config
        {
            int chCtrl;
            int chData;
            volatile void *txFIFO;
            int dreq;
        };

        using Configs = std::array<Config, N_TMDS_LANES>;

        // dma_channel_hw_t の先頭 4word
        struct Reg
        {
            const void *read_addr;
            volatile void *write_addr;
            uint32_t transfer_count;
            dma_channel_config ch_cfg;

            void set(const DMA::Config &cfg, const void *readAddr, int count, int readRingSizeLog2, bool irq);
        };

        enum class SyncLaneChunk
        {
            FRONT_PORCH,
            SYNC_DATA_ISLAND, // leading guardband, header, trailing guardband
            SYNC,
            BACK_PORCH,
            VIDEO_GUARDBAND,
            VIDEO,
            N,
        };
        enum class NonSyncLaneChunk
        {
            CTL0,
            PREAMBLE_TO_DATA,
            DATA_ISLAND, // leading guardband, packet, trailing guardband
            CTL1,
            PREAMBLE_TO_VIDEO,
            VIDEO_GUARDBAND,
            VIDEO,
            N,
        };
        // guardband は video の先頭に入れるが、timing 定義的には backporch 区間の長さに含める
        // backporch は 4+8+2 pixel 以上必要

        static inline constexpr int SYNC_LANE_CHUNKS = static_cast<int>(SyncLaneChunk::N);
        static inline constexpr int NO_SYNC_LANE_CHUNKS = static_cast<int>(NonSyncLaneChunk::N);
        //        static inline constexpr int N_CHUNKS = std::max(SYNC_LANE_CHUNKS, NO_SYNC_LANE_CHUNKS);

        struct List
        {
            Reg lane0[SYNC_LANE_CHUNKS];
            Reg lane12[2][NO_SYNC_LANE_CHUNKS];

            Reg *get(int i)
            {
                return i == 0 ? lane0 : lane12[i - 1];
            };
            const Reg *get(int i) const
            {
                return i == 0 ? lane0 : lane12[i - 1];
            };

            void setupListForVBlank(const Timing &timing, const Configs &cfgs, bool vSyncAsserted);
            void setupListForActive(const Timing &timing, const Configs &cfgs, const uint32_t *tmds);
            void __not_in_flash_func(updateScanLineData)(const Timing &timing, const uint32_t *tmds);
            void __not_in_flash_func(updateDataIslandPtr)(const DataIslandStream *data);

            void __not_in_flash_func(load)(const Configs &cfgs) const;
        };

        Configs cfgs_;

        List listVBlankSync_;
        List listVBlankNoSync_;
        List listActive_;
        List listActiveError_;

        DataIslandStream nextDataStream_;
    };
}

#endif /* _64C895CE_5134_63A8_C843_0FADACBE65CD */
