/*******************************************************************************
 * Copyright (C) Dean Miller
 * All rights reserved.
 *
 * This program is open source software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "qpcpp.h"
#include "qp_extras.h"

#include "hsm_id.h"
#include "AOPedal.h"
#include "event.h"
#include "SeesawConfig.h"

#include "bsp_gpio.h"
#include "bsp_sercom.h"
#include "bsp_adc.h"

#if 0
#include "bsp_dma.h"
#endif

Q_DEFINE_THIS_FILE

using namespace FW;

#if CONFIG_PEDAL

static uint16_t last_adc[6]
static const uint8_t adc_channels[] = [CONFIG_PEDAL_A0_CHANNEL, CONFIG_PEDAL_A1_CHANNEL, CONFIG_PEDAL_A2_CHANNEL,
                                        CONFIG_PEDAL_A3_CHANNEL, CONFIG_PEDAL_A4_CHANNEL, CONFIG_PEDAL_A5_CHANNEL];

AOPedal::AOPedal(Sercom *sercom) :
    QActive((QStateHandler)&AOPedal::InitialPseudoState), 
    m_id(AO_PEDAL), m_name("Pedal"),m_syncTimer(this, PEDAL_SYNC), m_sercom(sercom) {}

QState AOPedal::InitialPseudoState(AOPedal * const me, QEvt const * const e) {
    (void)e;

    me->subscribe(PEDAL_START_REQ);
    me->subscribe(PEDAL_STOP_REQ);
    me->subscribe(PEDAL_SYNC);
      
    return Q_TRAN(&AOPedal::Root);
}

QState AOPedal::Root(AOPedal * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_EVENT(e);
            status = Q_HANDLED();
            break;
        }
        case Q_EXIT_SIG: {
            LOG_EVENT(e);
            status = Q_HANDLED();
            break;
        }
        case Q_INIT_SIG: {
            status = Q_TRAN(&AOPedal::Stopped);
            break;
        }
		case PEDAL_STOP_REQ: {
			LOG_EVENT(e);
			status = Q_TRAN(&AOPedal::Stopped);
			break;
		}
        default: {
            status = Q_SUPER(&QHsm::top);
            break;
        }
    }
    return status;
}

QState AOPedal::Stopped(AOPedal * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_EVENT(e);
            status = Q_HANDLED();
            break;
        }
        case Q_EXIT_SIG: {
            LOG_EVENT(e);
            status = Q_HANDLED();
            break;
        }
        case PEDAL_STOP_REQ: {
            LOG_EVENT(e);
            Evt const &req = EVT_CAST(*e);
            Evt *evt = new PedalStopCfm(req.GetSeq(), ERROR_SUCCESS);
            QF::PUBLISH(evt, me);
            status = Q_HANDLED();
            break;
        }
        case PEDAL_START_REQ: {
            LOG_EVENT(e);

            pinPeripheral(CONFIG_PEDAL_PIN_MOSI, CONFIG_PEDAL_MUX);
			//pinPeripheral(CONFIG_PEDAL_PIN_MISO, CONFIG_PEDAL_MUX);
			pinPeripheral(CONFIG_PEDAL_PIN_SCK, CONFIG_PEDAL_MUX);
			//pinPeripheral(CONFIG_PEDAL_PIN_SS, CONFIG_PEDAL_MUX);

			initSPI( me->m_sercom, CONFIG_PEDAL_PAD_TX, CONFIG_PEDAL_PAD_RX, CONFIG_PEDAL_CHAR_SIZE,CONFIG_PEDAL_DATA_ORDER);
			setClockModeSPI( me->m_sercom, SERCOM_SPI_MODE_3);
			
            //set inputs
            uint32_t mask = (1ul << CONFIG_PEDAL_BTN_PIN) | (1ul << CONFIG_PEDAL_FS1_PIN) 
                            | (1ul << CONFIG_PEDAL_FS2_PIN) | (1ul << CONFIG_PEDAL_START_PIN);
			gpio_dirclr_bulk(PORTA, mask);
			gpio_pullenset_bulk(mask);
			gpio_outset_bulk(PORTA, mask);

            //set outputs
            mask = (1ul << CONFIG_PEDAL_RELAY_SET_PIN) | (1ul << CONFIG_PEDAL_RELAY_RST_PIN);
			gpio_dirset_bulk(PORTA, mask);
			gpio_outclr_bulk(PORTA, mask);

            mask = (1ul << CONFIG_PEDAL_PIN_SS);
			gpio_dirset_bulk(PORTA, mask);
			gpio_outset_bulk(PORTA, mask);

            adc_init();

            //TODO: midi init
#if 0
            dmac_init();
            dmac_alloc(CONFIG_PEDAL_DMAC_CHANNEL);

            dmac_set_action(CONFIG_PEDAL_DMAC_CHANNEL, DMA_TRIGGER_ACTON_BEAT);
            dmac_set_trigger(CONFIG_PEDAL_DMAC_CHANNEL, CONFIG_PEDAL_DMAC_TRIGGER);

            dmac_set_descriptor(
	              CONFIG_PEDAL_DMAC_CHANNEL,
	              (void *)&m_pedalState,
	              (void *)&me->m_sercom->SPI.DATA.reg,
	              sizeof(struct pedalState),
	              DMA_BEAT_SIZE_BYTE,
	              true,
	              false);
#endif

			Evt const &req = EVT_CAST(*e);
			Evt *evt = new PedalStartCfm(req.GetSeq(), ERROR_SUCCESS);
			QF::PUBLISH(evt, me);
			
			status = Q_TRAN(&AOPedal::Started);
            break;
        }
        default: {
            status = Q_SUPER(&AOPedal::Root);
            break;
        }
    }
    return status;
}

QState AOPedal::Started(AOPedal * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_EVENT(e);
			me->m_syncTimer.armX(CONFIG_PEDAL_SYNC_INTERVAL, CONFIG_PEDAL_SYNC_INTERVAL);
            status = Q_HANDLED();
            break;
        }
        case Q_EXIT_SIG: {
            LOG_EVENT(e);
            me->m_syncTimer.disarm();
            status = Q_HANDLED();
            break;
        }
		case PEDAL_STOP_REQ: {
			LOG_EVENT(e);
			Evt const &req = EVT_CAST(*e);
			Evt *evt = new PedalStopCfm(req.GetSeq(), ERROR_SUCCESS);
			QF::PUBLISH(evt, me);
			status = Q_TRAN(AOPedal::Stopped);
			break;
		}
        case PEDAL_SYNC: {
            LOG_EVENT(e);

            //read alt button

            //read all ADC, record if changed

            //read footswitches

            //send all data
            status = Q_HANDLED();
            break;
        }
        default: {
            status = Q_SUPER(&AOPedal::Root);
            break;
        }
    }
    return status;
}

#endif
