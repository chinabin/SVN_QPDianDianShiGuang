#include "GameHandler.h"
#include "Desk.h"
#include "Card.h"
#include "LTime.h"
#include "LVideo.h"
#include "LLog.h"
#include "Config.h"
#include "RoomVip.h"
#include "Work.h"

enum GangThink {
	GangThink_qianggang,
	GangThink_gangshangpao,
	GangThink_over,
};

class GameHandler_JingMen : public GameHandler
{
public:
	GameHandler_JingMen()
	{
		LLOG_DEBUG("GameHandler_JingMen");
		shutdown();
	}
	bool startup(Desk *desk)
	{
		LLOG_DEBUG("GameHandler_JingMen startup");
		if (desk)
		{
			shutdown();
			m_desk = desk;
			m_playtype.clear();
		}
		return true;
	}

	void shutdown(void)
	{
		LLOG_DEBUG("GameHandler_JingMen shutdown");
		m_desk = NULL;
		m_curOutCard = NULL;
		m_curGetCard = NULL;
		for (Lint i = 0; i< DESK_USER_COUNT; i++)
		{
			m_thinkInfo[i].Reset();
			m_thinkRet[i].Clear();
			m_handCard[i].clear();
			m_outCard[i].clear();
			m_pengCard[i].clear();
			m_minggangCard[i].clear();
			m_angangCard[i].clear();
			m_eatCard[i].clear();
			m_angang[i] = 0;
			m_minggang[i] = 0;
			m_playerHuInfo[i] = 0;
			m_playerBombInfo[i] = 0;
			m_first_turn[i] = false;
			m_LouHuState[i] = false;
		}
		m_GangThink = GangThink_over;
		m_beforePos = 0;
		m_beforeType = 0;
		m_gold = 0;
		m_zhuangpos = 0;
		memset(&m_WangBaCard, 0, sizeof(m_WangBaCard));
		m_curPos = 0;
		m_needGetCard = false;
		m_deskCard.clear();
		mGameInfo.m_gangcard.clear();
	}

	bool getFanpaiCard(CardVector& cardVec, Card& result)
	{
		Lsize vecSz = m_deskCard.size();
		bool bfind = false;
		for (Lsize i = 0; i < 5; ++i)
		{
			Lint seed = L_Rand(0, vecSz - 1);
			Card* fanpai = m_deskCard[seed];
			if (fanpai->m_color == 4)
			{
				continue;
			}

			result.m_color = fanpai->m_color;
			result.m_number = fanpai->m_number;
			bfind = true;
			break;
		}

		if (!bfind)
		{
			for (Lsize i = vecSz-1; i > 0; --i)
			{
				Card* fanpai = m_deskCard[i];
				if (fanpai->m_color == 4)
				{
					continue;
				}

				result.m_color = fanpai->m_color;
				result.m_number = fanpai->m_number;
				bfind == true;
				break;
			}
		}

		if (!bfind)
		{
			LLOG_ERROR("getFanpaiCard cannot find size %d", vecSz);
			return false;
		}

		return true;
	}

	void DeakCard()
	{
		LLOG_DEBUG("GameHandler_JingMen DeakCard");
		if (!m_desk || !m_desk->m_vip)
		{
			return;
		}
		for (Lint i = 0; i < DESK_USER_COUNT; ++i)
		{
			m_handCard[i].clear();
			m_outCard[i].clear();
			m_pengCard[i].clear();
			m_minggangCard[i].clear();
			m_angangCard[i].clear();
			m_eatCard[i].clear();
			m_thinkInfo[i].Reset();
			m_thinkRet[i].Clear();
			m_first_turn[i] = true;
			m_LouHuState[i] = false;
		}
		m_GangThink = GangThink_over;
		m_deskCard.clear();
		m_curOutCard = NULL;//当前出出来的牌
		m_curGetCard = NULL;
		m_needGetCard = false;
		m_curPos = m_zhuangpos;
		m_beforePos = INVAILD_POS;
		mGameInfo.m_gangcard.clear();

		memset(m_diangangRelation, 0, sizeof(m_diangangRelation));
		memset(&m_WangBaCard, 0, sizeof(m_WangBaCard));
		memset(m_angang, 0, sizeof(m_angang));//暗杠数量
		memset(m_minggang, 0, sizeof(m_minggang));
		memset(m_playerHuInfo, 0, sizeof(m_playerHuInfo));
		memset(m_playerBombInfo, 0, sizeof(m_playerBombInfo));
		memset(m_hongZhongCount, 0, sizeof(m_hongZhongCount));
		memset(m_laiziCount, 0, sizeof(m_laiziCount));

		//发牌   
		if (gConfig.GetDebugModel() && (m_desk->m_specialCard[0].m_color > 0 && m_desk->m_specialCard[0].m_number > 0))   //玩家指定发牌 牌局
		{
			gCardMgr.DealCard2(m_handCard, m_desk->m_desk_user_count, m_deskCard, m_desk->m_specialCard, m_desk->getGameType(), m_playtype);
		}
		else                //正常随机发牌 牌局
		{
			gCardMgr.DealCard(m_handCard, m_desk->m_desk_user_count, m_deskCard, m_desk->getGameType(), m_playtype);
		}

		//庄家多发一张牌
		Card* newCard = m_deskCard.back();
		m_handCard[m_curPos].push_back(newCard);
		m_deskCard.pop_back();

		//发送消息给客户端
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			if (m_desk->m_user[i] != NULL)
			{
				LMsgS2CPlayStart msg;
				msg.m_zhuang = m_curPos;
				msg.m_pos = i;
				for (int x = 0; x<m_desk->m_desk_user_count; x++)
				{
					msg.m_score.push_back(m_desk->m_vip->m_score[x]);
				}
				for (Lsize j = 0; j < m_handCard[i].size(); ++j)
				{
					msg.m_cardValue[j].m_number = m_handCard[i][j]->m_number;
					msg.m_cardValue[j].m_color = m_handCard[i][j]->m_color;
				}

				for (Lint j = 0; j < m_desk->m_desk_user_count; ++j)
				{
					msg.m_cardCount[j] = m_handCard[j].size();
				}
				msg.m_dCount = (Lint)m_deskCard.size();
				m_desk->m_user[i]->Send(msg);
			}
		}

		//录像功能
		m_video.Clear();
		Lint id[4];
		Ldouble score[4];
		memset(id, 0, sizeof(id));
		memset(score, 0, sizeof(score));
		std::vector<CardValue> vec[4];
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			id[i] = m_desk->m_user[i]->GetUserDataId();
			score[i] = m_desk->m_vip->GetUserScore(m_desk->m_user[i]);
			for (Lint j = 0; j < m_handCard[i].size(); ++j)
			{
				CardValue v;
				v.m_color = m_handCard[i][j]->m_color;
				v.m_number = m_handCard[i][j]->m_number;
				vec[i].push_back(v);
			}
		}
		m_video.DealCard(id, vec, gWork.GetCurTime().Secs(), m_zhuangpos, score, m_desk->GetDeskId(), m_desk->m_vip->m_curCircle, m_desk->m_vip->m_maxCircle, m_desk->m_flag, &m_desk->getPlayType());

		//开王霸
		if (!m_deskCard.empty())
		{
			Card wangba;

			if (gConfig.GetDebugModel() && (m_desk->m_specialCard[0].m_color > 0 && m_desk->m_specialCard[0].m_number > 0))
			{
				Lsize cardsz = m_deskCard.size();
				for (size_t i = 0; i < cardsz; ++i)
				{
					if (m_desk->m_specialCard[0].m_color != 4)
					{
						wangba.m_color = m_desk->m_specialCard[0].m_color;
						wangba.m_number = m_desk->m_specialCard[0].m_number;
						break;
					}

					if (m_deskCard[i]->m_color != 4)
					{
						wangba.m_color = m_deskCard[i]->m_color;
						wangba.m_number = m_deskCard[i]->m_number;
						break;
					}
				}
				
			}
			else
			{
				getFanpaiCard(m_deskCard, wangba);
			}

			if (wangba.m_number == 0 || wangba.m_color == 4)
			{
				wangba.m_color = 1;
				wangba.m_number = 9;
			}

			LMsgS2CWangBa msg;
			msg.m_Wang_Ba.m_color = wangba.m_color;
			msg.m_Wang_Ba.m_number = wangba.m_number;
			m_WangBaCard = wangba;
			m_desk->BoadCast(msg);
		}
		std::vector<CardValue> wangbaVector;
		CardValue wangbaValue;
		wangbaValue.m_color = m_WangBaCard.m_color;
		wangbaValue.m_number = m_WangBaCard.m_number;
		wangbaVector.push_back(wangbaValue);
		m_video.AddOper(VIDEO_OPEN_WANGBA, 0, wangbaVector);
		LLOG_DEBUG("GameHandler_JingMen 开王霸 c:%d,n:%d", m_WangBaCard.m_color,m_WangBaCard.m_number);
	}

	void SetDeskPlay()
	{
		LLOG_DEBUG("GameHandler_JingMen SetDeskPlay");
		if (!m_desk || !m_desk->m_vip)
		{
			return;
		}
		if (m_desk->m_vip)
			m_desk->m_vip->SendInfo();

		m_desk->setDeskState(DESK_PLAY);
		DeakCard();
		CheckStartPlayCard();
	}
	void ProcessRobot(Lint pos, User * pUser)
	{
		if (pos < 0 || pos > 3)
		{
			return;
		}
		switch (m_desk->getDeskPlayState())
		{
		case DESK_PLAY_GET_CARD:
		{
			//打出牌去
			if (m_desk->getDeskPlayState() == DESK_PLAY_GET_CARD && m_curPos == pos)
			{
				LMsgC2SUserPlay msg;
				msg.m_thinkInfo.m_type = THINK_OPERATOR_OUT;
				
				CardVector & tmp_handcard = m_handCard[pos];
				Lsize tmp_sz = tmp_handcard.size();

				CardValue card;

				bool bhongzhong = false;
				int laizNum = 0;
				for (size_t i = 0; i < tmp_sz; ++i)
				{
					Card * tmp_card = m_handCard[pos][i];
					if (tmp_card->m_color == 4 && tmp_card->m_number == 5)
					{
						card.m_color = tmp_card->m_color;
						card.m_number = tmp_card->m_number;
						bhongzhong = true;
						break;
					}

					if (tmp_card->m_color == m_WangBaCard.m_color && tmp_card->m_number == m_WangBaCard.m_number)
					{
						if (laizNum)
						{
							card.m_color = tmp_card->m_color;
							card.m_number = tmp_card->m_number;
							++laizNum;
							break;
						}
						++laizNum;
					}

					//if (tmp_card->m_color == 3 && tmp_card->m_number == 6)
					//{
					//	card.m_color = tmp_card->m_color;
					//	card.m_number = tmp_card->m_number;
					//	++laizNum;
					//	++laizNum;
					//	//break;
					//}
				}

				if (!bhongzhong)
				{
					for (size_t i = 0; i < tmp_sz; ++i)
					{
						Card * tmp_card = m_handCard[pos][i];
						/*if (tmp_card->m_color == 4 && tmp_card->m_number == 5)
						{
							card.m_color = tmp_card->m_color;
							card.m_number = tmp_card->m_number;
							bhongzhong = true;
							break;
						}*/

						/*if (tmp_card->m_color == m_WangBaCard.m_color && tmp_card->m_number == m_WangBaCard.m_number)
						{
						if (laizNum)
						{
						card.m_color = tmp_card->m_color;
						card.m_number = tmp_card->m_number;
						++laizNum;
						break;
						}
						++laizNum;
						}*/

						if (tmp_card->m_color == 3 && tmp_card->m_number == 6)
						{
							card.m_color = tmp_card->m_color;
							card.m_number = tmp_card->m_number;
							++laizNum;
							++laizNum;
							//break;
						}
					}
				}
				

				if (bhongzhong || laizNum > 1)
				{
					msg.m_thinkInfo.m_card.push_back(card);
					m_desk->HanderUserPlayCard(pUser, &msg);
					return;
				}
				
				card.m_color = m_handCard[pos][0]->m_color;
				card.m_number = m_handCard[pos][0]->m_number;

				msg.m_thinkInfo.m_card.push_back(card);
				m_desk->HanderUserPlayCard(pUser, &msg);
			}
		}
		break;
		case DESK_PLAY_THINK_CARD:
		{
			if (m_thinkInfo[pos].NeedThink())
			{
				for (int i = 0; i < m_thinkInfo[pos].m_thinkData.size(); i++)
				{
					if (m_thinkInfo[pos].m_thinkData[i].m_type == THINK_OPERATOR_BOMB)
					{
						LMsgC2SUserOper msg;
						msg.m_think.m_type = THINK_OPERATOR_NULL;		//
						std::vector<Card*>& mCard = m_thinkInfo[pos].m_thinkData[i].m_card;
						for (int j = 0; j < mCard.size(); j++)
						{
							CardValue card;
							card.m_color = mCard[j]->m_color;
							card.m_number = mCard[j]->m_number;
							msg.m_think.m_card.push_back(card);
						}
						m_desk->HanderUserOperCard(pUser, &msg);
						return;
					}
					else
					{
						LMsgC2SUserOper msg;
						msg.m_think.m_type = m_thinkInfo[pos].m_thinkData[i].m_type;
						std::vector<Card*>& mCard = m_thinkInfo[pos].m_thinkData[i].m_card;
						for (int j = 0; j < mCard.size(); j++)
						{
							CardValue card;
							card.m_color = mCard[j]->m_color;
							card.m_number = mCard[j]->m_number;
							msg.m_think.m_card.push_back(card);
						}
						m_desk->HanderUserOperCard(pUser, &msg);
					}
				}
			}
		}
		break;
		default:
		{

		}
		}
	}

	void HanderUserPlayCard(User* pUser, LMsgC2SUserPlay* msg)
	{
		LLOG_DEBUG("GameHandler_JingMen HanderUserPlayCard");
		if (m_desk == NULL || pUser == NULL || msg == NULL)
		{
			LLOG_DEBUG("HanderUserEndSelect NULL ERROR ");
			return;
		}
		LMsgS2CUserPlay sendMsg;
		sendMsg.m_errorCode = 0;
		sendMsg.m_pos = m_curPos;
		sendMsg.m_cs_card = msg->m_thinkInfo;

		Lint pos = m_desk->GetUserPos(pUser);
		if (pos == INVAILD_POS)
		{
			//pUser->Send(sendMsg);
			LLOG_DEBUG("HanderUserPlayCard pos error %s", pUser->m_userData.m_nike.c_str());
			return;
		}

		if (pos != m_curPos)
		{
			pUser->Send(sendMsg);
			LLOG_ERROR("HanderUserPlayCard not my pos %d:%d", pos, m_curPos);
			return;
		}

		if (msg->m_thinkInfo.m_type == THINK_OPERATOR_OUT)
		{
			if (m_thinkInfo[pos].NeedThink())
			{
				VideoDoing(99, pos, 0, 0);
			}
			if (msg->m_thinkInfo.m_card.size())
			{
				CardValue & msgCard = msg->m_thinkInfo.m_card[0];
				for (Lsize i = 0; i < m_handCard[pos].size(); ++i)
				{
					Card * t_curOutCard = m_handCard[pos][i];
					if (t_curOutCard->m_color == msgCard.m_color && t_curOutCard->m_number == msgCard.m_number)
					{
						if ((msgCard.m_color == 4 && msgCard.m_number ==5) || (msgCard.m_color == m_WangBaCard.m_color && msgCard.m_number == m_WangBaCard.m_number))
						{
							{
								gCardMgr.EraseCard(m_handCard[pos], t_curOutCard);
								m_desk->BoadCast(sendMsg);
								m_outCard[pos].push_back(t_curOutCard);

								//录像
								std::vector<CardValue> cards;
								CardValue card;
								card.m_color = t_curOutCard->m_color;
								card.m_number = t_curOutCard->m_number;
								cards.push_back(card);
								m_video.AddOper(VIDEO_OPER_OUT_CARD, pos, cards);
							}

							if ((msgCard.m_color == 4 && msgCard.m_number == 5))
							{
								++m_hongZhongCount[pos];
							}
							else
							{
								++m_laiziCount[pos];
							}

							//检查是否是最后一张牌结束
							SetPlayIng(pos, true, true, true, true);
							return;
						}


						m_curOutCard = m_handCard[pos][i];
						gCardMgr.EraseCard(m_handCard[pos], m_curOutCard);
						m_desk->BoadCast(sendMsg);
						m_beforePos = pos;
						m_beforeType = THINK_OPERATOR_OUT;

						//录像
						std::vector<CardValue> cards;
						CardValue card;
						card.m_color = m_curOutCard->m_color;
						card.m_number = m_curOutCard->m_number;
						cards.push_back(card);
						m_video.AddOper(VIDEO_OPER_OUT_CARD, pos, cards);

						//这里玩家思考
						SetThinkIng();
						break;
					}
				}
			}
			return;
		}


		ThinkUnit* unit = NULL;
		for (Lint i = 0; i < m_thinkInfo[pos].m_thinkData.size(); ++i)
		{
			if (msg->m_thinkInfo.m_type == m_thinkInfo[pos].m_thinkData[i].m_type)
			{
				if (msg->m_thinkInfo.m_card.size() == m_thinkInfo[pos].m_thinkData[i].m_card.size())
				{
					bool find = true;
					for (Lsize j = 0; j < msg->m_thinkInfo.m_card.size(); ++j)
					{
						if (msg->m_thinkInfo.m_card[j].m_color != m_thinkInfo[pos].m_thinkData[i].m_card[j]->m_color ||
							msg->m_thinkInfo.m_card[j].m_number != m_thinkInfo[pos].m_thinkData[i].m_card[j]->m_number)
						{
							find = false;
							break;
						}
					}

					if (find)
					{
						unit = &m_thinkInfo[pos].m_thinkData[i];
						break;
					}
				}
			}
		}

		if (unit)
		{
			if (unit->m_type == THINK_OPERATOR_BOMB)
			{
				//录相;
				VideoDoing(unit->m_type, pos, 0, 0);
				if (m_curGetCard)
				{
					gCardMgr.EraseCard(m_handCard[pos], m_curGetCard);
					sendMsg.m_huCard.m_color = m_curGetCard->m_color;
					sendMsg.m_huCard.m_number = m_curGetCard->m_number;
				}
				sendMsg.m_hu.push_back(unit->m_hu.front());
				sendMsg.m_cardCount = m_handCard[pos].size();
				for (Lint i = 0; i < sendMsg.m_cardCount; i++)
				{
					CardValue mCard;
					mCard.m_color = m_handCard[pos][i]->m_color;
					mCard.m_number = m_handCard[pos][i]->m_number;
					sendMsg.m_cardValue.push_back(mCard);
				}
				m_desk->BoadCast(sendMsg);

				m_thinkRet[m_curPos] = m_thinkInfo[m_curPos].m_thinkData[0];
				m_playerHuInfo[m_curPos] = WIN_SUB_ZIMO;
				CardVector winCards[DESK_USER_COUNT];
				for (int i = 0; i <m_desk->m_desk_user_count; i++)
				{
					if (m_thinkRet[i].m_type == THINK_OPERATOR_BOMB)
					{
						winCards[i] = m_thinkRet[i].m_card;
					}
				}
				OnGameOver(WIN_ZIMO, m_playerHuInfo, INVAILD_POS, winCards);
			}
			else if (unit->m_type == THINK_OPERATOR_ABU)
			{
				//录相;
				VideoDoing(unit->m_type, pos, unit->m_card[0]->m_color, unit->m_card[0]->m_number);
				//
				gCardMgr.EraseCard(m_handCard[pos], unit->m_card[0], 4);
				m_desk->BoadCast(sendMsg);
				m_angang[pos] += 1;
				//录像
				std::vector<CardValue> cards;
				for (int i = 0; i < 4; i++)
				{
					CardValue card;
					card.m_color = unit->m_card[0]->m_color;
					card.m_number = unit->m_card[0]->m_number;
					cards.push_back(card);
					m_angangCard[pos].push_back(unit->m_card[0]);
				}
				m_video.AddOper(VIDEO_OPER_AN_BU, pos, cards);
				//这里玩家思考
				m_beforePos = pos;
				m_beforeType = THINK_OPERATOR_ABU;
				SetPlayIng(pos, true, true, true, true);
			}
			else if (unit->m_type == THINK_OPERATOR_MBU)
			{
				//录相;
				VideoDoing(unit->m_type, pos, unit->m_card[0]->m_color, unit->m_card[0]->m_number);
				//
				gCardMgr.EraseCard(m_pengCard[pos], unit->m_card[0], 3);
				gCardMgr.EraseCard(m_handCard[pos], unit->m_card[0], 1);
				m_desk->BoadCast(sendMsg);
				m_minggang[pos] += 1;
				//录像
				std::vector<CardValue> cards;
				for (int i = 0; i < 4; i++)
				{
					CardValue card;
					card.m_color = unit->m_card[0]->m_color;
					card.m_number = unit->m_card[0]->m_number;
					cards.push_back(card);
					m_minggangCard[pos].push_back(unit->m_card[0]);
				}
				m_video.AddOper(VIDEO_OPER_SELF_BU, pos, cards);
				//这里玩家思考
				m_beforePos = pos;
				m_beforeType = THINK_OPERATOR_MBU;
				SetPlayIng(pos, true, true, true, true);
			}
		}
		else
		{
			LLOG_DEBUG("Desk::HanderUserPlayCard %s,%d", pUser->m_userData.m_nike.c_str(), msg->m_thinkInfo.m_type);
		}
	}

	void HanderUserOperCard(User* pUser, LMsgC2SUserOper* msg)
	{
		LLOG_DEBUG("GameHandler_JingMen HanderUserOperCard");
		LMsgS2CUserOper sendMsg;
		sendMsg.m_pos = m_curPos;
		sendMsg.m_think = msg->m_think;

		Lint pos = m_desk->GetUserPos(pUser);
		if (pos == INVAILD_POS || !m_thinkInfo[pos].NeedThink())
		{
			sendMsg.m_errorCode = 1;
			pUser->Send(sendMsg);
			return;
		}

		bool find = false;
		for (Lsize i = 0; i < m_thinkInfo[pos].m_thinkData.size(); ++i)
		{
			if (m_thinkInfo[pos].m_thinkData[i].m_type == msg->m_think.m_type)
			{
				if (m_thinkInfo[pos].m_thinkData[i].m_card.size() == msg->m_think.m_card.size())
				{
					bool check = true;
					for (Lsize j = 0; j < msg->m_think.m_card.size(); ++j)
					{
						if (msg->m_think.m_card[j].m_color != m_thinkInfo[pos].m_thinkData[i].m_card[j]->m_color ||
							msg->m_think.m_card[j].m_number != m_thinkInfo[pos].m_thinkData[i].m_card[j]->m_number)
						{
							check = false;
							break;
						}
					}

					if (check)
					{
						m_thinkRet[pos] = m_thinkInfo[pos].m_thinkData[i];
						find = true;
						break;
					}
				}
			}
		}

		if (!find)
		{
			m_thinkRet[pos].m_type = THINK_OPERATOR_NULL;
			if (m_thinkInfo[pos].HasHu())//漏胡
			{
				m_LouHuState[pos] = true;
			}
		}

		//录相;
		VideoDoing(msg->m_think.m_type, pos, (msg->m_think.m_card.size()>0) ? msg->m_think.m_card[0].m_color : 0, (msg->m_think.m_card.size()>0) ? msg->m_think.m_card[0].m_number : 0);

		if (msg->m_think.m_type == THINK_OPERATOR_BOMB)
		{
			LMsgS2CUserOper send;
			send.m_pos = pos;
			send.m_errorCode = 0;
			send.m_think = msg->m_think;
			send.m_card.m_color = (m_curOutCard == NULL) ? 1 : m_curOutCard->m_color;		//临时的 有错误
			send.m_card.m_number = (m_curOutCard == NULL) ? 1 : m_curOutCard->m_number;		//临时的 有错误
			send.m_hu.push_back(m_thinkRet[pos].m_hu.front());
			send.m_cardCount = m_handCard[pos].size();
			for (Lint i = 0; i < send.m_cardCount; i++)
			{
				CardValue mCard;
				mCard.m_color = m_handCard[pos][i]->m_color;
				mCard.m_number = m_handCard[pos][i]->m_number;
				send.m_cardValue.push_back(mCard);
			}
			m_desk->BoadCast(send);
		}

		//设置以及思考过了
		m_thinkInfo[pos].Reset();

		CheckThink();
	}

	void OnUserReconnect(User* pUser)
	{
		LLOG_DEBUG("GameHandler_JingMen OnUserReconnect");
		if (pUser == NULL || m_desk == NULL)
		{
			return;
		}
		//发送当前圈数信息
		if (m_desk->m_vip)
			m_desk->m_vip->SendInfo();
		Lint pos = m_desk->GetUserPos(pUser);
		if (pos == INVAILD_POS)
		{
			LLOG_ERROR("Desk::OnUserReconnect pos error %d", pUser->GetUserDataId());
			return;
		}
		Lint nCurPos = m_curPos;
		Lint nDeskPlayType = m_desk->getDeskPlayState();
		LMsgS2CDeskState reconn;
		reconn.m_user_count = m_desk->m_desk_user_count;
		reconn.m_state = m_desk->getDeskState();
		reconn.m_pos = nCurPos;
		reconn.m_time = 15;
		reconn.m_zhuang = m_zhuangpos;
		reconn.m_myPos = pos;
		if (nCurPos != pos)
		{
			reconn.m_flag = 0;
		}
		else
		{
			reconn.m_flag = 1;			//不知道对不对
		}
		reconn.m_dCount = m_deskCard.size();
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			reconn.m_cardCount[i] = m_handCard[i].size();
			reconn.m_oCount[i] = m_outCard[i].size();
			reconn.m_aCount[i] = m_angangCard[i].size();
			reconn.m_mCount[i] = m_minggangCard[i].size();
			reconn.m_pCount[i] = m_pengCard[i].size();
			reconn.m_eCount[i] = m_eatCard[i].size();
			reconn.m_score[i] = m_desk->m_vip->m_score[i];

			for (Lsize j = 0; j < m_outCard[i].size(); ++j)
			{
				reconn.m_oCard[i][j].m_color = m_outCard[i][j]->m_color;
				reconn.m_oCard[i][j].m_number = m_outCard[i][j]->m_number;
			}

			for (Lsize j = 0; j < m_angangCard[i].size(); ++j)
			{
				reconn.m_aCard[i][j].m_color = m_angangCard[i][j]->m_color;
				reconn.m_aCard[i][j].m_number = m_angangCard[i][j]->m_number;
			}

			for (Lsize j = 0; j < m_minggangCard[i].size(); ++j)
			{
				reconn.m_mCard[i][j].m_color = m_minggangCard[i][j]->m_color;
				reconn.m_mCard[i][j].m_number = m_minggangCard[i][j]->m_number;
			}

			for (Lsize j = 0; j < m_pengCard[i].size(); ++j)
			{
				reconn.m_pCard[i][j].m_color = m_pengCard[i][j]->m_color;
				reconn.m_pCard[i][j].m_number = m_pengCard[i][j]->m_number;
			}

			for (Lsize j = 0; j < m_eatCard[i].size(); ++j)
			{
				reconn.m_eCard[i][j].m_color = m_eatCard[i][j]->m_color;
				reconn.m_eCard[i][j].m_number = m_eatCard[i][j]->m_number;
			}
		}

		//我的牌,客户的重连，之前莫得牌的重新拿出来发给他
		if (nDeskPlayType == DESK_PLAY_GET_CARD && m_needGetCard && pos == nCurPos)
		{
			CardVector tmp = m_handCard[pos];
			if (m_curGetCard)
			{
				reconn.m_cardCount[pos] -= 1;
				gCardMgr.EraseCard(tmp, m_curGetCard);
			}
			for (Lsize j = 0; j < tmp.size(); ++j)
			{
				reconn.m_cardValue[j].m_color = tmp[j]->m_color;
				reconn.m_cardValue[j].m_number = tmp[j]->m_number;
			}
		}
		else
		{
			for (Lsize j = 0; j < m_handCard[pos].size(); ++j)
			{
				reconn.m_cardValue[j].m_color = m_handCard[pos][j]->m_color;
				reconn.m_cardValue[j].m_number = m_handCard[pos][j]->m_number;
			}
		}

		//该出牌的玩家，多发一张牌，用于打出去。
		if (m_needGetCard && nDeskPlayType == DESK_PLAY_THINK_CARD)
		{
			if (m_curOutCard && pos != nCurPos)
			{
				reconn.m_cardCount[nCurPos] ++;
			}
			else if (m_curOutCard&&pos == m_beforePos&& m_beforeType == THINK_OPERATOR_OUT)
			{
				reconn.m_cardCount[pos]++;
				//reconn.m_oCount[pos]--;
				CardValue xx;
				xx.m_color = m_curOutCard->m_color;
				xx.m_number = m_curOutCard->m_number;
				reconn.m_cardValue[reconn.m_cardCount[pos] - 1] = xx;
			}
		}
		reconn.m_WangbaCard.m_color = m_WangBaCard.m_color;
		reconn.m_WangbaCard.m_number = m_WangBaCard.m_number;
		pUser->Send(reconn);
	
		//我思考
		if (nDeskPlayType == DESK_PLAY_THINK_CARD)
		{
			if (m_thinkInfo[pos].NeedThink())
			{
				LMsgS2CThink think;
				think.m_time = 15;
				think.m_flag = 1;
				think.m_card.m_color = (m_curOutCard == NULL) ? 1 : m_curOutCard->m_color;		//临时的 有错误 m_curOutCard->m_color;
				think.m_card.m_number = (m_curOutCard == NULL) ? 1 : m_curOutCard->m_number;		//临时的 有错误m_curOutCard->m_number;
				for (Lsize j = 0; j < m_thinkInfo[pos].m_thinkData.size(); ++j)
				{
					ThinkData info;
					info.m_type = m_thinkInfo[pos].m_thinkData[j].m_type;
					for (Lsize n = 0; n < m_thinkInfo[pos].m_thinkData[j].m_card.size(); ++n)
					{
						CardValue v;
						v.m_color = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_color;
						v.m_number = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_number;
						info.m_card.push_back(v);
					}
					think.m_think.push_back(info);
				}
				pUser->Send(think);
			}
		}

		//我出牌
		if (nDeskPlayType == DESK_PLAY_GET_CARD && m_needGetCard && pos == nCurPos)
		{
			LMsgS2COutCard msg;
			msg.m_time = 15;
			msg.m_pos = pos;
			msg.m_deskCard = (Lint)m_deskCard.size();
			msg.m_flag = (m_curGetCard&&m_needGetCard) ? 0 : 1;
			msg.m_gang = 0;
			msg.m_end = m_deskCard.size() == 1 ? 1 : 0;
			if (m_needGetCard && m_curGetCard)
			{
				msg.m_curCard.m_color = m_curGetCard->m_color;
				msg.m_curCard.m_number = m_curGetCard->m_number;
			}

			for (Lsize j = 0; j < m_thinkInfo[pos].m_thinkData.size(); ++j)
			{
				ThinkData info;
				info.m_type = m_thinkInfo[pos].m_thinkData[j].m_type;
				for (Lsize n = 0; n < m_thinkInfo[pos].m_thinkData[j].m_card.size(); ++n)
				{
					CardValue v;
					v.m_color = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_color;
					v.m_number = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_number;
					info.m_card.push_back(v);
				}
				msg.m_think.push_back(info);
			}
			pUser->Send(msg);
		}

		//桌面上的牌重新发给玩家的桌牌
		if (m_needGetCard && nDeskPlayType == DESK_PLAY_THINK_CARD && m_GangThink != GangThink_qianggang)
		{
			if (m_curOutCard)
			{
				LMsgS2CUserPlay sendMsg;
				sendMsg.m_errorCode = 0;
				sendMsg.m_pos = nCurPos;
				sendMsg.m_cs_card.m_type = THINK_OPERATOR_OUT;
				CardValue card;
				card.m_color = m_curOutCard->m_color;
				card.m_number = m_curOutCard->m_number;
				sendMsg.m_cs_card.m_card.push_back(card);
				pUser->Send(sendMsg);
			}
		}
	}

	void CheckStartPlayCard()
	{
		LLOG_DEBUG("GameHandler_JingMen CheckStartPlayCard");
		//不用抓牌了，直接思考		//有BUG 思考时手里多张牌
		SetPlayIng(m_zhuangpos, false, false, true, true, true);
		m_curGetCard = m_handCard[m_curPos].back();
		m_needGetCard = true;
	}

	//摸牌
	void SetPlayIng(Lint pos, bool needGetCard, bool gang, bool needThink, bool canhu, bool first_think = false)
	{
		LLOG_DEBUG("GameHandler_JingMen SetPlayIng");
		if (m_desk == NULL)
		{
			LLOG_DEBUG("HanderUserEndSelect NULL ERROR ");
			return;
		}
		if (pos < 0 || pos >= INVAILD_POS)
		{
			LLOG_ERROR("Desk::SetPlayIng pos error ！");
			return;
		}
		if (m_first_turn[pos] && needThink && !gang)
		{
			mGameInfo.m_first_turn = true;
			m_first_turn[pos] = false;
			if (pos == m_zhuangpos)
			{
				mGameInfo.m_tianhu = true;
			}
		}
		else {
			mGameInfo.m_first_turn = false;
			mGameInfo.m_tianhu = false;
		}
		//穿庄
		if (m_deskCard.empty() && needGetCard)
		{
			OnGameOver(WIN_NONE, m_playerHuInfo, INVAILD_POS, NULL);
			return;
		}
		m_curPos = pos;

		//我摸牌思考信息
		m_thinkInfo[pos].m_thinkData.clear();
		m_desk->setDeskPlayState(DESK_PLAY_GET_CARD);
		m_LouHuState[pos] = false;
		m_needGetCard = false;
		if (needGetCard)
		{
			m_needGetCard = true;
			m_curGetCard = m_deskCard.back();
			m_deskCard.pop_back();
			//录像
			std::vector<CardValue> cards;
			CardValue card;
			card.m_color = m_curGetCard->m_color;
			card.m_number = m_curGetCard->m_number;
			cards.push_back(card);
			m_video.AddOper(VIDEO_OPER_GET_CARD, pos, cards);
		}

		if (needThink)
		{
			mGameInfo.m_GameType = m_desk->getGameType();	// 0 湖南， 3， 长沙
			mGameInfo.b_canHu = canhu;		// 是否可以胡
			mGameInfo.b_onlyHu = false;
			mGameInfo.b_canEat = false;		// 是否可以吃
			mGameInfo.m_wangBa = &m_WangBaCard;
			mGameInfo.m_getCardThink = true;
			mGameInfo.m_playerPos = m_curPos;	// 当前一个出牌位置
			mGameInfo.m_MePos = m_curPos;		// 玩家的位置
			mGameInfo.i_canHuScore = 2;
			mGameInfo.m_thinkGang = gang;	// 单独处理是不是杠的牌
			Lint maxscore = 0;
			for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
			{
				if (i == m_curPos)
				{
					continue;
				}

				Lint tmpscore = m_hongZhongCount[i] + m_laiziCount[i] * 2 + m_minggang[i] + m_angang[i] * 2;
				if (tmpscore > maxscore)
				{
					maxscore = tmpscore;
				}
			}
			mGameInfo.m_playerScore = maxscore + m_hongZhongCount[m_curPos] + m_laiziCount[m_curPos] * 2 + m_minggang[m_curPos] + m_angang[m_curPos] * 2;
			m_thinkInfo[pos].m_thinkData = gCardMgr.CheckGetCardOperator(m_handCard[pos], m_pengCard[pos], m_angangCard[pos], m_minggangCard[pos], m_eatCard[pos], m_curGetCard, mGameInfo);

			VideoThink(pos);
		}

		if (m_needGetCard)
		{
			m_handCard[pos].push_back(m_curGetCard);
			gCardMgr.SortCard(m_handCard[pos]);
		}

		for (Lint i = 0; i <m_desk->m_desk_user_count; ++i)
		{
			if (m_desk->m_user[i] != NULL)
			{
				LMsgS2COutCard msg;
				msg.m_time = 15;
				msg.m_pos = pos;
				msg.m_deskCard = (Lint)m_deskCard.size();
				msg.m_gang = 0;
				msg.m_end = m_desk->getDeskPlayState() == DESK_PLAY_END_CARD ? 1 : 0;
				msg.m_flag = 1;
				if (m_needGetCard)
				{
					msg.m_flag = 0;
				}

				if (pos == i)
				{

					if (m_needGetCard)
					{
						msg.m_curCard.m_number = m_curGetCard->m_number;
						msg.m_curCard.m_color = m_curGetCard->m_color;
					}

					for (Lsize j = 0; j < m_thinkInfo[pos].m_thinkData.size(); ++j)
					{
						LLOG_DEBUG("setPlaying thinktype:%d", m_thinkInfo[pos].m_thinkData[j].m_type);
						ThinkData info;
						info.m_type = m_thinkInfo[pos].m_thinkData[j].m_type;
						if (first_think&&info.m_type == THINK_OPERATOR_BOMB && !(m_handCard[pos].empty()))
						{
							if (m_handCard[pos].back())
							{
								m_thinkInfo[pos].m_thinkData[j].m_card.push_back(m_handCard[pos].back());
								msg.m_curCard.m_number = m_handCard[pos].back()->m_number;
								msg.m_curCard.m_color = m_handCard[pos].back()->m_color;
							}
						}
						for (Lsize n = 0; n < m_thinkInfo[pos].m_thinkData[j].m_card.size(); ++n)
						{
							CardValue v;
							v.m_color = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_color;
							v.m_number = m_thinkInfo[pos].m_thinkData[j].m_card[n]->m_number;
							info.m_card.push_back(v);
						}
						msg.m_think.push_back(info);
					}
				}
				m_desk->m_user[i]->Send(msg);
			}
		}
	}

	bool calcScore(Lint result, Lint finalWinPos, Lint bombpos, Lint gold[], Lint base_score, LMsgS2CGameOver& over)
	{
		LLOG_DEBUG("GameHandler_JingMen calcScore result=%d, gangfollowhu=%d", result,  m_playtype.H_GangFollowHu);
		if (!m_desk)
		{
			return false;
		}
		if (gold == NULL)
		{
			return false;
		}
		if (result == WIN_BOMB)
			if (bombpos<0 || bombpos>m_desk->m_desk_user_count - 1)
				return false;
		
		if (finalWinPos == INVAILD_POS)
		{
			return false;
		}

		if (result == WIN_NONE)
		{
			return false;
		}

		for (Lint i = 0; i < m_desk->GetUserCount(); ++i)
		{
			if (i != finalWinPos)
			{
				m_thinkRet[i].m_hu.clear();
			}
		}

		//计算胡牌得分
		Lint addScore = 0;
		for (std::vector<Lint>::iterator ithutype = m_thinkRet[finalWinPos].m_hu.begin(); ithutype != m_thinkRet[finalWinPos].m_hu.end(); ++ithutype)
		{
			LLOG_DEBUG("GameHandler_JingMen calcScore hutype:%d", *ithutype);
			if (*ithutype == HU_YINGHU)
			{
				addScore = 1;
			}
		}

		Lint winfan = addScore + m_hongZhongCount[finalWinPos] + m_laiziCount[finalWinPos] * 2 + m_minggang[finalWinPos] + m_angang[finalWinPos] * 2;
		if (result == WIN_ZIMO)
		{
			bool jingding = true;
			for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
			{
				if (i != finalWinPos)
				{
					Lint shufan = m_hongZhongCount[i] + m_laiziCount[i] * 2 + m_minggang[i] + m_angang[i] * 2;
					Lint totolfan = shufan + winfan;
					if (totolfan > 3)
					{
						totolfan = 4;
					}
					else
					{
						jingding = false;
					}
					gold[i] = totolfan;
				}
			}

			if (jingding)
			{
				gold[finalWinPos] = base_score * 20 * (m_desk->m_desk_user_count - 1);
				for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
				{
					if (i != finalWinPos)
					{
						gold[i] = -(base_score * 20);
					}
				}
			}
			else
			{
				gold[finalWinPos] = 0;
				for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
				{
					if (i != finalWinPos)
					{
						gold[finalWinPos] += base_score * pow(2, gold[i]);
						gold[i] = -(base_score * pow(2, gold[i]));
					}
				}
			}
		}

		if (result == WIN_BOMB)
		{
			Lint shufan = m_hongZhongCount[bombpos] + m_laiziCount[bombpos] * 2 + m_minggang[bombpos] + m_angang[bombpos] * 2;
			Lint totolfan = shufan + winfan;
			if (totolfan > 3)
			{
				totolfan = 4;
			}
			gold[finalWinPos] = Lint(base_score * pow(2, totolfan));
			gold[bombpos] = -(Lint(base_score * pow(2, totolfan)));
		}

		LLOG_DEBUG("GameHandler_JingMen calcScore gold:%d,%d,%d,%d", gold[0], gold[1], gold[2], gold[3]);
		return true;
	}

	
	void SetThinkIng()
	{
		LLOG_DEBUG("GameHandler_JingMen SetThinkIng");
		bool think = false;
		if (!m_desk)
			return;
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			m_thinkRet[i].Clear();
			m_thinkInfo[i].Reset();
			if (i != m_curPos)
			{
				mGameInfo.m_GameType = m_desk->getGameType();
				mGameInfo.m_tianhu = false;
				mGameInfo.m_GameType = m_desk->getGameType();

				if (m_first_turn[i])
				{
					m_first_turn[i] = false;
					mGameInfo.b_canEat = m_playtype.H_CanEat&&i == m_desk->GetNextPos(m_zhuangpos);		// 是否可以吃

				}
				else
				{
					mGameInfo.b_canEat = (m_playtype.H_CanEat&&i == m_desk->GetNextPos(m_beforePos));		// 是否可以吃
				}

				mGameInfo.m_thinkGang = false;
				mGameInfo.b_canHu = (m_playtype.H_DianPao && !m_LouHuState[i]);
				mGameInfo.m_first_turn = false;
				mGameInfo.m_wangBa = &m_WangBaCard;
				mGameInfo.i_canHuScore = 2;
				mGameInfo.m_playerScore = m_hongZhongCount[i] + m_laiziCount[i] * 2 + m_minggang[i] + m_angang[i] * 2 + m_hongZhongCount[m_curPos] + m_laiziCount[m_curPos] * 2 + m_minggang[m_curPos] + m_angang[m_curPos] * 2;
				mGameInfo.b_guoGang == false;
				mGameInfo.m_playerPos = m_curPos;	// 当前一个出牌位置
				mGameInfo.m_MePos = i;		// 玩家的位置

				LLOG_DEBUG("setThinking pos:%d, dianpao:%d , louhu:%d", i, m_playtype.H_DianPao, m_curOutCard->m_color * 10 + m_curOutCard->m_number);
				m_thinkInfo[i].m_thinkData = gCardMgr.CheckOutCardOperator(m_handCard[i], m_pengCard[i], m_angangCard[i], m_minggangCard[i], m_eatCard[i], m_curOutCard, mGameInfo);
				if (m_thinkInfo[i].NeedThink())
				{
					think = true;
					VideoThink(i);
				}
			}
		}

		if (think)
		{
			m_desk->setDeskPlayState(DESK_PLAY_THINK_CARD);
			for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
			{
				LMsgS2CThink think;
				think.m_time = 15;
				think.m_card.m_color = m_curOutCard->m_color;
				think.m_card.m_number = m_curOutCard->m_number;
				if (m_thinkInfo[i].NeedThink())
				{
					think.m_flag = 1;
					for (Lsize j = 0; j < m_thinkInfo[i].m_thinkData.size(); ++j)
					{
						ThinkData info;
						info.m_type = m_thinkInfo[i].m_thinkData[j].m_type;
						LLOG_DEBUG("Think pos：%d， thinkType：%d", i, info.m_type);
						for (Lsize n = 0; n < m_thinkInfo[i].m_thinkData[j].m_card.size(); ++n)
						{
							CardValue v;
							v.m_color = m_thinkInfo[i].m_thinkData[j].m_card[n]->m_color;
							v.m_number = m_thinkInfo[i].m_thinkData[j].m_card[n]->m_number;
							info.m_card.push_back(v);
						}
						think.m_think.push_back(info);
					}
				}
				else
				{
					think.m_flag = 0;
				}
				m_desk->m_user[i]->Send(think);
			}
		}
		else
		{
			ThinkEnd();
		}
	}

	void CheckThink()
	{
		LLOG_DEBUG("GameHandler_JingMen CheckThink");
		if (!m_desk)
			return;
		bool hu = false;
		bool Peng = false;
		bool Chi = false;
		bool Bu = false;
		bool hu_New = false;
		bool Peng_New = false;
		bool Chi_New = false;
		bool Bu_New = false;
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			//
			if (m_thinkRet[i].m_type == THINK_OPERATOR_BOMB)			hu = true;
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_PENG)		Peng = true;
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_CHI)		Chi = true;
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_MBU)		Bu = true;

			//
			if (m_thinkInfo[i].NeedThink())
			{
				if (m_thinkInfo[i].HasHu())				hu_New = true;
				else if (m_thinkInfo[i].HasPeng())		Peng_New = true;
				else if (m_thinkInfo[i].HasChi())		Chi_New = true;
				else if (m_thinkInfo[i].HasMBu())		Bu_New = true;
			}
		}

		bool think = false;
		if (hu_New)
		{
			think = true;
		}
		else
		{
			if (!hu)
			{
				if (Peng_New || Bu_New)
					think = true;
				else
				{
					if (!(Peng || Bu))
					{
						if (Chi_New)
							think = true;
					}
				}
			}
		}

		if (!think)
			ThinkEnd();
	}

	void ThinkEnd()
	{
		LLOG_DEBUG("GameHandler_JingMen ThinkEnd");
		if (!m_desk)
			return;
		for (int i = 0; i <m_desk->m_desk_user_count; i++)
		{
			if (m_thinkInfo[i].NeedThink())
			{
				VideoDoing(99, i, 0, 0);
			}
			m_thinkInfo[i].Reset();
		}
		if (m_GangThink == GangThink_qianggang)
		{
			m_GangThink = GangThink_gangshangpao;
		}
		else if (m_GangThink == GangThink_gangshangpao)
		{
			m_GangThink = GangThink_over;
		}
		Lint huCount = 0;
		Lint pengPos = INVAILD_POS;
		Lint gangPos = INVAILD_POS;
		Lint chiPos = INVAILD_POS;
		WIN_TYPE winType = WIN_BOMB;
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			if (m_thinkRet[i].m_type == THINK_OPERATOR_BOMB)
			{
				huCount++;
				m_playerHuInfo[i] = WIN_SUB_BOMB;
				m_playerBombInfo[m_beforePos] = WIN_SUB_ABOMB;
				if (m_thinkRet[i].m_hu.size() == 0)
				{
					continue;
				}
				for (std::vector<Lint>::iterator it = m_thinkRet[i].m_hu.begin(); it != m_thinkRet[i].m_hu.end(); ++it)
				{
					if (*it == HU_QiShou4WangBa)
					{
						winType = WIN_ZIMO;
						break;
					}
				}
			}
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_MBU)
				gangPos = i;
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_PENG)
				pengPos = i;
			else if (m_thinkRet[i].m_type == THINK_OPERATOR_CHI)
				chiPos = i;
		}

		if (huCount != 0)
		{
			CardVector winCards[DESK_USER_COUNT];
			for (int i = 0; i < m_desk->m_desk_user_count; i++)
			{
				if (m_thinkRet[i].m_type == THINK_OPERATOR_BOMB)
				{
					winCards[i] = m_thinkRet[i].m_card;
				}
			}
			OnGameOver(winType, m_playerHuInfo, m_beforePos, winCards);
			return;
		}

		//明杠
		if (gangPos != INVAILD_POS)
		{
			LMsgS2CUserOper send;
			send.m_pos = gangPos;
			send.m_errorCode = 0;
			send.m_think.m_type = m_thinkRet[gangPos].m_type;
			for (Lsize i = 0; i < m_thinkRet[gangPos].m_card.size(); ++i)
			{
				CardValue v;
				v.m_color = m_thinkRet[gangPos].m_card[i]->m_color;
				v.m_number = m_thinkRet[gangPos].m_card[i]->m_number;
				send.m_think.m_card.push_back(v);
			}
			send.m_card.m_color = m_curOutCard->m_color;
			send.m_card.m_number = m_curOutCard->m_number;
			m_desk->BoadCast(send);
			gCardMgr.EraseCard(m_handCard[gangPos], m_thinkRet[gangPos].m_card[0], 3);

			//录像
			std::vector<CardValue> cards;
			for (int i = 0; i < 4; i++)
			{
				CardValue card;
				card.m_color = m_curOutCard->m_color;
				card.m_number = m_curOutCard->m_number;
				cards.push_back(card);
				m_minggangCard[gangPos].push_back(m_curOutCard);
			}
			m_video.AddOper(VIDEO_OPER_OTHER_BU, gangPos, cards);
			m_minggang[gangPos] += 1;
			m_diangangRelation[m_beforePos][gangPos]++;
			for (int i = 0; i < m_desk->m_desk_user_count; i++)
			{
				m_thinkRet[i].Clear();
			}
			//给玩家摸一张牌
			SetPlayIng(gangPos, true, true, true, true);
			return;
		}


		if (pengPos != INVAILD_POS)
		{
			LMsgS2CUserOper send;
			send.m_pos = pengPos;
			send.m_errorCode = 0;
			send.m_think.m_type = m_thinkRet[pengPos].m_type;
			for (Lsize i = 0; i < m_thinkRet[pengPos].m_card.size(); ++i)
			{
				CardValue v;
				v.m_color = m_thinkRet[pengPos].m_card[i]->m_color;
				v.m_number = m_thinkRet[pengPos].m_card[i]->m_number;
				send.m_think.m_card.push_back(v);
			}
			send.m_card.m_color = m_curOutCard->m_color;
			send.m_card.m_number = m_curOutCard->m_number;
			m_desk->BoadCast(send);
			gCardMgr.EraseCard(m_handCard[pengPos], m_curOutCard, 2);

			//录像
			std::vector<CardValue> cards;
			for (int i = 0; i < 3; i++)
			{
				CardValue card;
				card.m_color = m_curOutCard->m_color;
				card.m_number = m_curOutCard->m_number;
				cards.push_back(card);
				m_pengCard[pengPos].push_back(m_curOutCard);
			}
			m_video.AddOper(VIDEO_OPER_PENG_CARD, pengPos, cards);

			for (int i = 0; i <m_desk->m_desk_user_count; i++)
			{
				m_thinkRet[i].Clear();
			}
			//碰完打一张牌
			m_curGetCard = NULL;
			SetPlayIng(pengPos, false, false, true, false);
			m_needGetCard = true;
			return;
		}

		//吃
		if (chiPos != INVAILD_POS)
		{
			LMsgS2CUserOper send;
			send.m_pos = chiPos;
			send.m_errorCode = 0;
			send.m_think.m_type = m_thinkRet[chiPos].m_type;
			for (Lsize i = 0; i < m_thinkRet[chiPos].m_card.size(); ++i)
			{
				CardValue v;
				v.m_color = m_thinkRet[chiPos].m_card[i]->m_color;
				v.m_number = m_thinkRet[chiPos].m_card[i]->m_number;
				send.m_think.m_card.push_back(v);
			}

			if (m_curOutCard&&m_thinkRet[chiPos].m_card.size()>2 && m_thinkRet[chiPos].m_card[0] && m_thinkRet[chiPos].m_card[1] && m_thinkRet[chiPos].m_card[2])
			{
				send.m_card.m_color = m_curOutCard->m_color;
				send.m_card.m_number = m_curOutCard->m_number;
				m_desk->BoadCast(send);
				gCardMgr.EraseCard(m_handCard[chiPos], m_thinkRet[chiPos].m_card[0], 1);
				gCardMgr.EraseCard(m_handCard[chiPos], m_thinkRet[chiPos].m_card[1], 1);

				//录像
				std::vector<CardValue> cards;
				//手牌
				CardValue card;
				card.m_color = m_thinkRet[chiPos].m_card[0]->m_color;
				card.m_number = m_thinkRet[chiPos].m_card[0]->m_number;
				cards.push_back(card);
				//吃的牌放中间
				card.m_color = m_curOutCard->m_color;
				card.m_number = m_curOutCard->m_number;
				cards.push_back(card);
				//手牌
				card.m_color = m_thinkRet[chiPos].m_card[1]->m_color;
				card.m_number = m_thinkRet[chiPos].m_card[1]->m_number;
				cards.push_back(card);

				m_video.AddOper(VIDEO_OPER_EAT, chiPos, cards);

				m_eatCard[chiPos].push_back(m_thinkRet[chiPos].m_card[0]);
				m_eatCard[chiPos].push_back(m_curOutCard);
				m_eatCard[chiPos].push_back(m_thinkRet[chiPos].m_card[1]);

				for (int i = 0; i <m_desk->m_desk_user_count; i++)
				{
					m_thinkRet[i].Clear();
				}
				//给玩家摸一张牌
				m_curGetCard = NULL;
				SetPlayIng(chiPos, false, false, false, false);
				m_needGetCard = true;
				return;
			}
			else
			{
				LLOG_ERROR("unknow error 101");
				return;
			}
		}

		//这里没有人操作
		if (m_beforeType == THINK_OPERATOR_ABU || m_beforeType == THINK_OPERATOR_MBU)
		{
			if (m_beforeType == THINK_OPERATOR_MBU)
			{
				m_minggang[m_beforePos] += 1;

				//录像
				std::vector<CardValue> cards;
				for (int i = 0; i < 4; i++)
				{
					CardValue card;
					card.m_color = m_curOutCard->m_color;
					card.m_number = m_curOutCard->m_number;
					cards.push_back(card);
				}
				m_video.AddOper(VIDEO_OPER_SELF_BU, m_beforePos, cards);
				m_minggangCard[m_beforePos].push_back(m_curOutCard);
				CardVector::iterator it = m_pengCard[m_beforePos].begin();
				for (; it != m_pengCard[m_beforePos].end(); it += 3)
				{
					if (gCardMgr.IsSame(m_curOutCard, *it))
					{
						m_minggangCard[m_beforePos].insert(m_minggangCard[m_beforePos].end(), it, it + 3);
						m_pengCard[m_beforePos].erase(it, it + 3);
						break;
					}
				}
			}
			else
			{
				m_angang[m_beforePos] += 1;

				gCardMgr.EraseCard(m_handCard[m_beforePos], m_curOutCard, 4);

				//录像
				std::vector<CardValue> cards;
				for (int i = 0; i < 4; i++)
				{
					CardValue card;
					card.m_color = m_curOutCard->m_color;
					card.m_number = m_curOutCard->m_number;
					cards.push_back(card);
					m_angangCard[m_beforePos].push_back(m_curOutCard);
				}
				m_video.AddOper(VIDEO_OPER_AN_BU, m_beforePos, cards);
			}

			//这里发暗杠 明杠消息
			LMsgS2CUserPlay sendMsg;
			sendMsg.m_errorCode = 0;
			sendMsg.m_pos = m_beforePos;
			sendMsg.m_cs_card.m_type = m_beforeType;
			CardValue card;
			card.m_color = m_curOutCard->m_color;
			card.m_number = m_curOutCard->m_number;
			sendMsg.m_cs_card.m_card.push_back(card);
			m_desk->BoadCast(sendMsg);
			SetPlayIng(m_beforePos, true, true, true, true);
		}
		else
		{
			m_outCard[m_beforePos].push_back(m_curOutCard);
			SetPlayIng(m_desk->GetNextPos(m_beforePos), true, false, true, true);
		}
	}

	void OnGameOver(Lint result, Lint winpos[], Lint bombpos, CardVector winCards[])
	{
		LLOG_DEBUG("GameHandler_JingMen OnGameOver winpos:%d,%d,%d,%d", winpos[0], winpos[1], winpos[2], winpos[3]);
		if (m_desk == NULL || m_desk->m_vip == NULL)
		{
			LLOG_DEBUG("OnGameOver NULL ERROR ");
			return;
		}

		//计算输赢结果
		Lint gold[4] = { 0 };
		Lint winPos = INVAILD_POS;

		//广播结果
		LMsgS2CGameOver over;
		over.m_user_count = m_desk->m_desk_user_count;

		//找出优先级最高的胡牌位置。
		Lint bombCount = 0;
		Lint tmppos;
		Lint base_score = 1;
		if (m_beforePos == INVAILD_POS)
		{
			tmppos = m_curPos;
		}
		else
		{
			tmppos = m_desk->GetNextPos(m_beforePos);
		}

		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			LLOG_DEBUG("GameHandler_JingMen OnGameOver tmppos:%d", tmppos);
			if (winpos[tmppos] == WIN_SUB_ZIMO || winpos[tmppos] == WIN_SUB_BOMB)
			{
				bombCount++;
				winPos = tmppos;
				break;
			}
			tmppos = m_desk->GetNextPos(tmppos);
		}
		LLOG_DEBUG("GameHandler_JingMen OnGameOver winPos:%d", winPos);

		Lint zhuangPos = m_zhuangpos;
		//计算庄
		if (result == WIN_ZIMO || result == WIN_BOMB)
		{
			if (winCards)
			{
				if (winPos != INVAILD_POS)
				{
					m_zhuangpos = winPos;
					//录像
					std::vector<CardValue> cards;
					for (Lint i = 0; i < winCards[m_zhuangpos].size(); i++)
					{
						CardValue curGetCard;
						curGetCard.m_color = winCards[m_zhuangpos][i]->m_color;
						curGetCard.m_number = winCards[m_zhuangpos][i]->m_number;
						cards.push_back(curGetCard);
					}
					VIDEO_OPER_CS vOper;
					vOper = result == WIN_ZIMO ? VIDEO_OPER_ZIMO : VIDEO_OPER_SHOUPAO;
					m_video.AddOper(vOper, m_zhuangpos, cards);

					//计算分数
					calcScore(result, winPos, bombpos, gold, base_score, over);

				}
			}
		}
		else
		{
			m_zhuangpos = m_curPos;
			//录像
			std::vector<CardValue> cards;
			m_video.AddOper(VIDEO_OPER_HUANGZHUANG, m_curPos, cards);
		}

		//保存录像
		m_video.m_Id = gVipLogMgr.GetVideoId();
		m_video.m_playType = m_desk->getPlayType();

		over.m_result = result;
		memcpy(over.m_cs_win, winpos, sizeof(over.m_cs_win));
		memcpy(over.m_score, gold, sizeof(over.m_score));
		memcpy(over.m_agang, m_angang, sizeof(over.m_agang));
		memcpy(over.m_mgang, m_minggang, sizeof(over.m_mgang));
		memcpy(over.m_dmgang, m_laiziCount, sizeof(over.m_dmgang));
		memcpy(over.m_dgang, m_hongZhongCount, sizeof(over.m_dgang));

		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			over.m_count[i] = m_handCard[i].size();

			//手牌
			for (Lint j = 0; j < over.m_count[i]; ++j)
			{
				over.m_card[i][j].m_color = m_handCard[i][j]->m_color;
				over.m_card[i][j].m_number = m_handCard[i][j]->m_number;
			}

			//胡牌类型
			if (m_thinkRet[i].m_type == THINK_OPERATOR_BOMB)
			{
				over.m_hu[i] = m_thinkRet[i].m_hu;
			}
		}
		if (winCards)
		{
			if (winPos != INVAILD_POS)
			{
				for (Lint i = 0; i < winCards[winPos].size(); i++)
				{
					CardValue curGetCard;
					curGetCard.m_color = winCards[winPos][i]->m_color;
					curGetCard.m_number = winCards[winPos][i]->m_number;
					over.m_hucards[winPos].push_back(curGetCard);
				}
			}
		}
		m_desk->SetDeskWait();
		Lint mgang[4] = { 0 };
		for (Lint i = 0; i < m_desk->m_desk_user_count; ++i)
		{
			mgang[i] += m_minggang[i];
		}

		//保存结果	
		Ldouble cal_gold[4] = { 0,0,0,0 };

		//保存结果	
		m_desk->m_vip->AddLog(m_desk->m_user, gold, cal_gold, winpos, zhuangPos, m_angang, mgang, m_playerBombInfo, m_video.m_Id, m_video.m_time);

		LMsgL2LDBSaveVideo video;
		video.m_type = 0;
		m_video.SetEveryResult(4, gold[0], gold[1], gold[2], gold[3]);
		m_video.SetEveryResult2(4, cal_gold[0], cal_gold[1], cal_gold[2], cal_gold[3]);
		video.m_sql = m_video.GetInsertSql();
		gWork.SendMsgToDb(video);

		//是否最后一局
		over.m_end = m_desk->m_vip->isEnd() ? 1 : 0;
		over.m_WangbaCard.m_color = m_WangBaCard.m_color;
		over.m_WangbaCard.m_number = m_WangBaCard.m_number;
		m_desk->BoadCast(over);

		m_desk->HanderGameOver();

		//////////////////////////////////////////////////////////////////////////

	}

	void VideoThink(Lint pos)
	{
		LLOG_DEBUG("GameHandler_JingMen VideoThink");
		if (m_thinkInfo[pos].m_thinkData.size() >0)
		{
			std::vector<CardValue> cards;
			for (auto itr = m_thinkInfo[pos].m_thinkData.begin(); itr != m_thinkInfo[pos].m_thinkData.end(); itr++)
			{
				CardValue card;
				card.m_number = itr->m_type;
				if (itr->m_card.size()>0)
					card.m_color = itr->m_card[0]->m_color * 10 + itr->m_card[0]->m_number;
				if (itr->m_card.size()>1)
					card.m_color = card.m_color * 1000 + itr->m_card[1]->m_color * 10 + itr->m_card[1]->m_number;
				cards.push_back(card);
			}
			m_video.AddOper(VIDEO_OPEN_THINK, pos, cards);
		}
	}

	void VideoDoing(Lint op, Lint pos, Lint card_color, Lint card_number)
	{
		LLOG_DEBUG("GameHandler_JingMen VideoDoing");
		std::vector<CardValue> cards;
		CardValue card;
		card.m_number = op;
		card.m_color = card_color * 10 + card_number;
		cards.push_back(card);
		m_video.AddOper(VIDEO_OPEN_DOING, pos, cards);
	}

	void SetPlayType(std::vector<Lint>& l_playtype) 
	{
		LLOG_DEBUG("GameHandler_JingMen SetPlayType");
		m_playtype.init_playtype_info(l_playtype);
		if (m_desk)
		{
			m_desk->m_desk_user_count = 4;
		}
	}

	//////////////////////////////////////////////////////////////////////////
public:
	Card			m_WangBaCard;	//王霸牌

private:
	Desk			*m_desk;
	Card*			m_curOutCard;//当前出出来的牌
	Card*			m_curGetCard;//当前获取的牌
	ThinkTool		m_thinkInfo[DESK_USER_COUNT];//当前打牌思考状态
	ThinkUnit		m_thinkRet[DESK_USER_COUNT];//玩家返回思考结果

	CardVector		m_handCard[DESK_USER_COUNT];//玩家手上的
	CardVector		m_outCard[DESK_USER_COUNT];	//玩家出的牌
	CardVector		m_pengCard[DESK_USER_COUNT];//玩家碰的牌，
	CardVector		m_minggangCard[DESK_USER_COUNT];//玩家明杠的牌
	CardVector		m_angangCard[DESK_USER_COUNT];//玩家暗杠的牌
	CardVector		m_eatCard[DESK_USER_COUNT];//玩家吃的牌
	Lint			m_angang[DESK_USER_COUNT];//暗杠数量
	Lint			m_minggang[DESK_USER_COUNT];//明杠数量
	Lint			m_diangangRelation[DESK_USER_COUNT][DESK_USER_COUNT];//A给B点杠的次数
	Lint			m_playerHuInfo[DESK_USER_COUNT];	//玩家胡牌信息， 因为可以多胡 放炮的人只保存了位置
	Lint			m_playerBombInfo[DESK_USER_COUNT];	//玩家放炮信息，[玩家位置对应的炮的类型]

	Lint            m_hongZhongCount[DESK_USER_COUNT]; //打出的红中的数量
	Lint            m_laiziCount[DESK_USER_COUNT]; //打出的癞子的数量

	Lint			m_beforePos;//之前操作的位置
	Lint			m_beforeType;//之前操作的类型
	Lint			m_gold;
	Lint			m_zhuangpos;//庄家位置
	Lint			m_curPos;						//当前操作玩家
	bool			m_needGetCard;
	CardVector		m_deskCard;		//桌子上剩余的牌
	VideoLog		m_video;						//录像
	OperateState	mGameInfo;
	PlayTypeInfo    m_playtype;
	GangThink		m_GangThink;	//当前是否在思考杠
	bool			m_first_turn[DESK_USER_COUNT]; //第一轮判断

	bool			m_LouHuState[DESK_USER_COUNT]; //玩家是否漏胡

};

DECLARE_GAME_HANDLER_CREATOR(JingMenMajiang, GameHandler_JingMen)