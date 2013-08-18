//
//  NetClient.cpp
//  OpenSpades
//
//  Created by yvt on 7/16/13.
//  Copyright (c) 2013 yvt.jp. All rights reserved.
//

#include "NetClient.h"
#include "../Core/Debug.h"
#include "../Core/Exception.h"
#include <vector>
#include "../Core/Debug.h"
#include "../Core/Math.h"
#include "World.h"
#include "Player.h"
#include "Client.h"
#include "Grenade.h"
#include "CTFGameMode.h"
#include "../Core/DeflateStream.h"
#include "../Core/MemoryStream.h"
#include "GameMap.h"
#include <string.h>
#include <math.h>
#include "TCGameMode.h"

namespace spades {
	namespace client {
		enum{
			BLUE_FLAG = 0,
			GREEN_FLAG = 1,
			BLUE_BASE = 2,
			GREEN_BASE = 3
		};
		enum PacketType {
			PacketTypePositionData = 0,
			PacketTypeOrientationData = 1,
			PacketTypeWorldUpdate = 2,
			PacketTypeInputData = 3,
			PacketTypeWeaponInput = 4,
			PacketTypeHitPacket = 5,		// C2S
			PacketTypeSetHP = 5,			// S2C
			PacketTypeGrenadePacket = 6,
			PacketTypeSetTool = 7,
			PacketTypeSetColour = 8,
			PacketTypeExistingPlayer = 9,
			PacketTypeShortPlayerData = 10,
			PacketTypeMoveObject = 11,
			PacketTypeCreatePlayer = 12,
			PacketTypeBlockAction = 13,
			PacketTypeBlockLine = 14,
			PacketTypeStateData = 15,
			PacketTypeKillAction = 16,
			PacketTypeChatMessage = 17,
			PacketTypeMapStart = 18,		// S2C
			PacketTypeMapChunk = 19,		// S2C
			PacketTypePlayerLeft = 20,		// S2P
			PacketTypeTerritoryCapture = 21,// S2P
			PacketTypeProgressBar = 22,
			PacketTypeIntelCapture = 23,	// S2P
			PacketTypeIntelPickup = 24,		// S2P
			PacketTypeIntelDrop = 25,		// S2P
			PacketTypeRestock = 26,			// S2P
			PacketTypeFogColour = 27,		// S2C
			PacketTypeWeaponReload = 28,	// C2S2P
			PacketTypeChangeTeam = 29,		// C2S2P
			PacketTypeChangeWeapon = 30,	// C2S2P
			
		};
		class NetPacketReader {
			std::vector<char> data;
			size_t pos;
		public:
			NetPacketReader(ENetPacket *packet){
				SPADES_MARK_FUNCTION();
				
				data.resize(packet->dataLength);
				memcpy(data.data(), packet->data, packet->dataLength);
				enet_packet_destroy(packet);
				pos = 1;
			}
			
			NetPacketReader(const std::vector<char> inData){
				data = inData;
				pos = 1;
			}
			PacketType GetType() {
				return (PacketType)data[0];
			}
			uint32_t ReadInt() {
				SPADES_MARK_FUNCTION();
				
				uint32_t value = 0;
				if(pos + 4 > data.size()){
					SPRaise("Received packet truncated");
				}
				value |= ((uint32_t)(uint8_t)data[pos++]);
				value |= ((uint32_t)(uint8_t)data[pos++]) << 8;
				value |= ((uint32_t)(uint8_t)data[pos++]) << 16;
				value |= ((uint32_t)(uint8_t)data[pos++]) << 24;
				return value;
			}
			uint16_t ReadShort() {
				SPADES_MARK_FUNCTION();
				
				uint32_t value = 0;
				if(pos + 2 > data.size()){
					SPRaise("Received packet truncated");
				}
				value |= ((uint32_t)(uint8_t)data[pos++]);
				value |= ((uint32_t)(uint8_t)data[pos++]) << 8;
				return (uint16_t)value;
			}
			uint8_t ReadByte() {
				SPADES_MARK_FUNCTION();
				
				if(pos >= data.size()){
					SPRaise("Received packet truncated");
				}
				return (uint8_t)data[pos++];
			}
			float ReadFloat() {
				SPADES_MARK_FUNCTION();
				union {
					float f;
					uint32_t v;
				};
				v = ReadInt();
				return f;
			}
			
			IntVector3 ReadIntColor() {
				SPADES_MARK_FUNCTION();
				IntVector3 col;
				col.z = ReadByte();
				col.y = ReadByte();
				col.x = ReadByte();
				return col;
			}
			
			Vector3 ReadFloatColor() {
				SPADES_MARK_FUNCTION();
				Vector3 col;
				col.z = ReadByte() / 255.f;
				col.y = ReadByte() / 255.f;
				col.x = ReadByte() / 255.f;
				return col;
			}
			
			std::vector<char> GetData() {
				return data;
			}
			
			std::string ReadData(size_t siz) {
				if(pos + siz > data.size()){
					SPRaise("Received packet truncated");
				}
				std::string s = std::string(data.data() + pos, siz);
				pos += siz;
				return s;
			}
			std::string ReadRemainingData() {
				return std::string(data.data() + pos,
								   data.size() - pos);
			}
			
			std::string ReadString(size_t siz){
				// convert to C string once so that
				// null-chars are removed
				return ReadData(siz).c_str(); // TODO: decode
			}
			std::string ReadRemainingString() {
				return ReadRemainingData(); // TODO: decode
			}
			
			void DumpDebug() {
#if 1
				printf("Packet 0x%02x [len=%d]", (int)GetType(),
					   (int)data.size());
				int bytes = (int)data.size();
				if(bytes > 64){
					bytes = 64;
				}
				for(int i = 0; i < bytes; i++)
					printf(" %02x", (unsigned int)(unsigned char)data[i]);
			
				printf("\n");
#endif
			}
		};
		
		class NetPacketWriter {
			std::vector<char> data;
		public:
			NetPacketWriter(PacketType type){
				data.push_back(type);
			}
			
			void Write(uint8_t v){
				SPADES_MARK_FUNCTION_DEBUG();
				data.push_back(v);
			}
			void Write(uint16_t v){
				SPADES_MARK_FUNCTION_DEBUG();
				data.push_back((char)(v));
				data.push_back((char)(v >> 8));
			}
			void Write(uint32_t v){
				SPADES_MARK_FUNCTION_DEBUG();
				data.push_back((char)(v));
				data.push_back((char)(v >> 8));
				data.push_back((char)(v >> 16));
				data.push_back((char)(v >> 24));
			}
			void Write(float v){
				SPADES_MARK_FUNCTION_DEBUG();
				union {
					float f; uint32_t i;
				};
				f = v;
				Write(i);
			}
			void WriteColor(IntVector3 v){
				Write((uint8_t)v.z);
				Write((uint8_t)v.y);
				Write((uint8_t)v.x);
			}
			
			void Write(std::string str){
				// TODO: encode from utf-8 to cp437
				data.insert(data.end(),
							str.begin(),
							str.end());
			}
			
			void Write(std::string str, size_t fillLen){
				// TODO: encode from utf-8 to cp437
				Write(str.substr(0, fillLen));
				size_t sz = str.size();
				while(sz < fillLen){
					Write((uint8_t)0);
					sz++;
				}
			}
			
			ENetPacket *CreatePacket(int flag = ENET_PACKET_FLAG_RELIABLE) {
				return enet_packet_create(data.data(),
										  data.size(),
										  flag);
			}
		};
		
		NetClient::NetClient(Client *c){
			SPADES_MARK_FUNCTION();
			
			client = c;
			
			enet_initialize();
			
			host = enet_host_create(NULL,
									1, 1,
									50000, 50000);
			if(!host){
				SPRaise("Failed to create ENet host");
			}
			
			if(enet_host_compress_with_range_coder(host) < 0)
				SPRaise("Failed to enable ENet Range coder.");
			
			peer = NULL;
			status = NetClientStatusNotConnected;
			
			savedPlayerPos.resize(32);
			savedPlayerFront.resize(32);
		}
		NetClient::~NetClient(){
			SPADES_MARK_FUNCTION();
			
			Disconnect();
			enet_host_destroy(host);
		}
		
		void NetClient::Connect(std::string hostname) {
			SPADES_MARK_FUNCTION();
			
			Disconnect();
			SPAssert(status == NetClientStatusNotConnected);
			
			if(hostname.find("aos:///") == 0){
				hostname = hostname.substr(7);
			}else if(hostname.find("aos://") == 0){
				hostname = hostname.substr(6);
			}
			
			ENetAddress address;
			std::string addr = hostname;
			size_t pos = hostname.find(':');
			if(pos == std::string::npos){
				addr = hostname;
				address.port = 32887;
			}else{
				address.port = atoi(hostname.substr(pos+1).c_str());
				addr = hostname.substr(0, pos);
			}
			
			if(addr.find('.') != std::string::npos){
				enet_address_set_host(&address, addr.c_str());
			}else{
				address.host = (uint32_t)atoll(addr.c_str());
			}
			
			savedPackets.clear();
			
			peer = enet_host_connect(host, &address, 1, 3);
			if(peer == NULL){
				SPRaise("Failed to create ENet peer");
			}
			
			status = NetClientStatusConnecting;
			statusString = "Connecting to the server";
			timeToTryMapLoad = 0;
		}
		
		void NetClient::Disconnect() {
			SPADES_MARK_FUNCTION();
			
			if(!peer)
				return;
			enet_peer_disconnect(peer, 0);
			
			status = NetClientStatusNotConnected;
			statusString = "Not connected";
			
			savedPackets.clear();
			
			ENetEvent event;
			while(enet_host_service(host, &event, 1000) > 0){
				switch(event.type){
					case ENET_EVENT_TYPE_RECEIVE:
						enet_packet_destroy(event.packet);
						break;
					case ENET_EVENT_TYPE_DISCONNECT:
						// disconnected safely
						// FIXME: release peer
						enet_peer_reset(peer);
						peer = NULL;
						return;
					default:;
						// discard
				}
			}
			
			enet_peer_reset(peer);
			// FXIME: release peer
			peer = NULL;
		}
		
		void NetClient::DoEvents(int timeout) {
			SPADES_MARK_FUNCTION();
			
			if(status == NetClientStatusNotConnected)
				return;
			
			ENetEvent event;
			while(enet_host_service(host, &event, timeout) > 0){
				if(event.type == ENET_EVENT_TYPE_DISCONNECT) {
					if(GetWorld()){
						client->SetWorld(NULL);
					}
					
					enet_peer_reset(peer);
					peer = NULL;
					status = NetClientStatusNotConnected;
					statusString = "Disconnected: " + DisconnectReasonString(event.data);
					SPRaise("Disconnected: %s", DisconnectReasonString(event.data).c_str());
				}
				if(status == NetClientStatusConnecting){
					if(event.type == ENET_EVENT_TYPE_CONNECT){
						statusString = "Awaiting for state";
					}else if(event.type == ENET_EVENT_TYPE_RECEIVE){
						NetPacketReader reader(event.packet);
						reader.DumpDebug();
						if(reader.GetType() != PacketTypeMapStart){
							SPRaise("Unexpeted packet: %d", (int)reader.GetType());
						}
						
						mapSize = reader.ReadInt();
						status = NetClientStatusReceivingMap;
						statusString = "Loading snapshot";
						timeToTryMapLoad = 30;
						tryMapLoadOnPacketType = true;
					}
				}else if(status == NetClientStatusReceivingMap){
					if(event.type == ENET_EVENT_TYPE_RECEIVE){
						NetPacketReader reader(event.packet);
						
						if(reader.GetType() == PacketTypeMapChunk){
							std::vector<char> dt = reader.GetData();
							dt.erase(dt.begin());
							mapData.insert(mapData.end(),
										   dt.begin(), dt.end());
							
							timeToTryMapLoad = 200;
							
							char buf[256];
							sprintf(buf, "Loading snapshot (%d/%d)",
									(int)mapData.size(), (int)mapSize);
							statusString = buf;
							
							if(mapSize == mapData.size()){
								status = NetClientStatusConnected;
								statusString = "Connected";
								
								try{
									MapLoaded();
								}catch(const std::exception& ex){
									if(strstr(ex.what(), "File truncated") ||
									   strstr(ex.what(), "EOF reached")){
										// hack: more data to load...
										status = NetClientStatusReceivingMap;
										statusString = "Still loading...";
									}else{
										Disconnect();
										statusString = "Error";
										throw;
									}
									
								}catch(...){
									Disconnect();
									statusString = "Error";
									throw;
								}
								
							}
							
						}else{
							reader.DumpDebug();
							
							if(reader.GetType() != PacketTypeWorldUpdate &&
							   reader.GetType() != PacketTypeExistingPlayer &&
							   reader.GetType() != PacketTypeCreatePlayer &&
							   tryMapLoadOnPacketType){
								status = NetClientStatusConnected;
								statusString = "Connected";
								
								try{
									MapLoaded();
								}catch(const std::exception& ex){
									tryMapLoadOnPacketType = false;
									if(strstr(ex.what(), "File truncated") ||
									   strstr(ex.what(), "EOF reached")){
										// hack: more data to load...
										status = NetClientStatusReceivingMap;
										statusString = "Still loading...";
										goto stillLoading;
									}else{
										Disconnect();
										statusString = "Error";
										throw;
									}
								}catch(...){
									Disconnect();
									statusString = "Error";
									throw;
								}
								Handle(reader);
							}else{
							stillLoading:
								savedPackets.push_back(reader.GetData());
							}
							
							//Handle(reader);
							
							
						}
					}
				}else if(status == NetClientStatusConnected){
					if(event.type == ENET_EVENT_TYPE_RECEIVE){
						NetPacketReader reader(event.packet);
						//reader.DumpDebug();
						try{
							Handle(reader);
						}catch(const std::exception& ex){
							int type = reader.GetType();
							reader.DumpDebug();
							SPRaise("Exception while handling packet type 0x%08x:\n%s",
									type, ex.what());
						}
					}
				}
			}
			
			if(status == NetClientStatusReceivingMap){
				if(timeToTryMapLoad > 0){
					timeToTryMapLoad--;
					if(timeToTryMapLoad == 0){
						try{
							MapLoaded();
						}catch(const std::exception& ex){
							if((strstr(ex.what(), "File truncated") ||
								strstr(ex.what(), "EOF reached")) &&
							   savedPackets.size() < 400){
								// hack: more data to load...
								status = NetClientStatusReceivingMap;
								statusString = "Still loading...";
								timeToTryMapLoad = 200;
							}else{
								Disconnect();
								statusString = "Error";
								throw;
							}
						}catch(...){
							Disconnect();
							statusString = "Error";
							throw;
						}
					}
				}
			}
		}
		
		World *NetClient::GetWorld(){
			return client->GetWorld();
		}
		
		Player * NetClient::GetPlayerOrNull(int pId){
			SPADES_MARK_FUNCTION();
			if(!GetWorld())
				SPRaise("Invalid Player ID %d: No world", pId);
			if(pId < 0 || pId >= GetWorld()->GetNumPlayerSlots())
				return NULL;
			return GetWorld()->GetPlayer(pId);
		}
		Player * NetClient::GetPlayer(int pId){
			SPADES_MARK_FUNCTION();
			if(!GetWorld())
				SPRaise("Invalid Player ID %d: No world", pId);
			if(pId < 0 || pId >= GetWorld()->GetNumPlayerSlots())
				SPRaise("Invalid Player ID %d: Out of range", pId);
			if(!GetWorld()->GetPlayer(pId))
				SPRaise("Invalid Player ID %d: Doesn't exist", pId);
			return GetWorld()->GetPlayer(pId);
		}
		
		Player *NetClient::GetLocalPlayer() {
			SPADES_MARK_FUNCTION();
			if(!GetWorld())
				SPRaise("Failed to get local player: no world");
			if(!GetWorld()->GetLocalPlayer())
				SPRaise("Failed to get local player: no local player");
			return GetWorld()->GetLocalPlayer();
		}
		
		Player *NetClient::GetLocalPlayerOrNull() {
			SPADES_MARK_FUNCTION();
			if(!GetWorld())
				SPRaise("Failed to get local player: no world");
			return GetWorld()->GetLocalPlayer();
		}
		PlayerInput ParsePlayerInput(uint8_t bits){
			PlayerInput inp;
			inp.moveForward = (bits & (1)) != 0;
			inp.moveBackward = (bits & (1 << 1)) != 0;
			inp.moveLeft = (bits & (1 << 2)) != 0;
			inp.moveRight = (bits & (1 << 3)) != 0;
			inp.jump = (bits & (1 << 4)) != 0;
			inp.crouch = (bits & (1 << 5)) != 0;
			inp.sneak = (bits & (1 << 6)) != 0;
			inp.sprint = (bits & (1 << 7)) != 0;
			return inp;
		}
		
		WeaponInput ParseWeaponInput(uint8_t bits){
			WeaponInput inp;
			inp.primary = ((bits & (1)) != 0);
			inp.secondary = ((bits & (1 << 1)) != 0);
			return inp;
		}
		
		std::string NetClient::DisconnectReasonString(enet_uint32 num){
			switch(num){
				case 1:
					return "You are banned from this server.";
				case 2:
					return "You were kicked from this server.";
				case 3:
					return "Incompatible client protocol version.";
				case 4:
					return "Server full";
				case 10:
					return "You were kicked from this server. (2)";
				default:
					return "Unknown";
			}
		}
		
		void NetClient::Handle(spades::client::NetPacketReader & reader) {
			SPADES_MARK_FUNCTION();
			
			switch(reader.GetType()){
				case PacketTypePositionData:
				{
					Player *p = GetLocalPlayer();
					Vector3 pos;
					if(reader.GetData().size() < 12){
						// sometimes 00 00 00 00 packet is sent.
						// ignore this now
						break;
					}
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat();
					p->SetPosition(pos);
				}
					break;
				case PacketTypeOrientationData:
				{
					Player *p = GetLocalPlayer();
					Vector3 pos;
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat();
					p->SetOrientation(pos);
				}
					break;
				case PacketTypeWorldUpdate:
					//reader.DumpDebug();
					for(int i = 0; i < 32; i++){
						Vector3 pos, front;
						pos.x = reader.ReadFloat();
						pos.y = reader.ReadFloat();
						pos.z = reader.ReadFloat();
						front.x = reader.ReadFloat();
						front.y = reader.ReadFloat();
						front.z = reader.ReadFloat();
						
						savedPlayerPos[i] = pos;
						savedPlayerFront[i] = front;
						if(pos.x != 0.f ||
						   pos.y != 0.f ||
						   pos.z != 0.f ||
						   front.x != 0.f ||
						   front.y != 0.f ||
						   front.z != 0.f){
							Player *p;
							SPAssert(!isnan(pos.x));
							SPAssert(!isnan(pos.y));
							SPAssert(!isnan(pos.z));
							SPAssert(!isnan(front.x));
							SPAssert(!isnan(front.y));
							SPAssert(!isnan(front.z));
							SPAssert(front.GetLength() < 40.f);
							if(GetWorld()){
								p = GetWorld()->GetPlayer(i);
								if(p){
									if(p != GetWorld()->GetLocalPlayer()){
										p->SetPosition(pos);
										p->SetOrientation(front);
									}
								}
							}
						}
					}
					SPAssert(reader.ReadRemainingData().empty());
					break;
				case PacketTypeInputData:
					if(!GetWorld())
						break;
				{
					int pId = reader.ReadByte();
					Player *p = GetPlayer(pId);
					
					PlayerInput inp = ParsePlayerInput(reader.ReadByte());
					
					if(GetWorld()->GetLocalPlayer() == p)
						break;
					
					p->SetInput(inp);
				}
					break;
					
				case PacketTypeWeaponInput:
					if(!GetWorld())
						break;
				{
					int pId = reader.ReadByte();
					Player *p = GetPlayer(pId);
					
					WeaponInput inp = ParseWeaponInput(reader.ReadByte());
					
					if(GetWorld()->GetLocalPlayer() == p)
						break;
					
					p->SetWeaponInput(inp);
				}
					break;
					
					// Hit Packet is Client-to-Server!
				case PacketTypeSetHP:
				{
					Player *p = GetLocalPlayer();
					int hp = reader.ReadByte();
					int type = reader.ReadByte(); // 0=fall, 1=weap
					Vector3 hurtPos;
					hurtPos.x = reader.ReadFloat();
					hurtPos.y = reader.ReadFloat();
					hurtPos.z = reader.ReadFloat();
					p->SetHP(hp, type ? HurtTypeWeapon:
							 HurtTypeFall, hurtPos);
				}
					break;
					
				case PacketTypeGrenadePacket:
					if(!GetWorld())
						break;
				{
					//reader.ReadByte(); // skip player Id
					Player *p = GetPlayerOrNull(reader.ReadByte());
					float fuseLen = reader.ReadFloat();
					Vector3 pos, vel;
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat();
					vel.x = reader.ReadFloat();
					vel.y = reader.ReadFloat();
					vel.z = reader.ReadFloat();
					
					if(p == GetLocalPlayerOrNull()){
						// local player's grenade is already
						// emit by Player
						break;
					}
					
					Grenade *g = new Grenade(GetWorld(),
											 pos, vel, fuseLen);
					GetWorld()->AddGrenade(g);
				}
					break;
					
				case PacketTypeSetTool:
				{
					Player *p = GetPlayer(reader.ReadByte());
					int tool = reader.ReadByte();
					switch(tool){
						case 0: p->SetTool(Player::ToolSpade); break;
						case 1: p->SetTool(Player::ToolBlock); break;
						case 2: p->SetTool(Player::ToolWeapon); break;
						case 3: p->SetTool(Player::ToolGrenade); break;
						default:
							SPRaise("Received invalid tool type: %d", tool);
					}
				}
					break;
				case PacketTypeSetColour:
				{
					Player *p = GetPlayerOrNull(reader.ReadByte());
					IntVector3 col = reader.ReadIntColor();
					if(p)
						p->SetHeldBlockColor(col);
					else
						temporaryPlayerBlockColor = col;
				}
					break;
				case PacketTypeExistingPlayer:
					if(!GetWorld())
						break;
				{
					int pId = reader.ReadByte();
					int team = reader.ReadByte();
					int weapon = reader.ReadByte();
					int tool = reader.ReadByte();
					int kills = reader.ReadInt(); 
					IntVector3 color = reader.ReadIntColor();
					std::string name = reader.ReadRemainingString();
					// TODO: decode name?
					
					WeaponType wType;
					switch(weapon){
						case 0:
							wType = RIFLE_WEAPON;
							break;
						case 1:
							wType = SMG_WEAPON;
							break;
						case 2:
							wType = SHOTGUN_WEAPON;
							break;
						default:
							SPRaise("Received invalid weapon: %d", weapon);
					}
					
					Player *p = new Player(GetWorld(), pId,
										   wType, team,
										   savedPlayerPos[pId],
										    GetWorld()->GetTeam(team).color);
					p->SetHeldBlockColor(color);
					//p->SetOrientation(savedPlayerFront[pId]);
					GetWorld()->SetPlayer(pId, p);
					
					switch(tool){
						case 0: p->SetTool(Player::ToolSpade); break;
						case 1: p->SetTool(Player::ToolBlock); break;
						case 2: p->SetTool(Player::ToolWeapon); break;
						case 3: p->SetTool(Player::ToolGrenade); break;
						default:
							SPRaise("Received invalid tool type: %d", tool);
					}
					
					World::PlayerPersistent& pers = GetWorld()->GetPlayerPersistent(pId);
					pers.name = name;
					pers.kills = kills;
				}
					break;
				case PacketTypeShortPlayerData:
					SPRaise("Unexpected: received Short Player Data");
				case PacketTypeMoveObject:
					if(!GetWorld()) SPRaise("No world");
				{
					uint8_t type = reader.ReadByte();
					uint8_t state = reader.ReadByte();
					Vector3 pos;
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat();
					
					CTFGameMode *ctf = dynamic_cast<CTFGameMode *>(GetWorld()->GetMode());
					if(ctf){
						switch(type){
							case BLUE_BASE:
								ctf->GetTeam(0).basePos = pos;
								break;
							case BLUE_FLAG:
								ctf->GetTeam(0).flagPos = pos;
								break;
							case GREEN_BASE:
								ctf->GetTeam(1).basePos = pos;
								break;
							case GREEN_FLAG:
								ctf->GetTeam(1).flagPos = pos;
								break;
						}
					}
					
					TCGameMode *tc = dynamic_cast<TCGameMode *>(GetWorld()->GetMode());
					if(tc){
						if(type >= tc->GetNumTerritories()){
							SPRaise("Invalid territory id specified: %d (max = %d)", (int)type, tc->GetNumTerritories() - 1);
						}
						
						if(state > 2){
							SPRaise("Invalid state %d specified for territory owner.", (int)state);
						}
						
						TCGameMode::Territory *t = tc->GetTerritory(type);
						t->pos = pos;
						t->ownerTeamId = state;/*
						t->progressBasePos = 0.f;
						t->progressRate = 0.f;
						t->progressStartTime = 0.f;
						t->capturingTeamId = -1;*/
					}
				}
					break;
				case PacketTypeCreatePlayer:
				{
					if(!GetWorld())
						SPRaise("No world");
					int pId = reader.ReadByte();
					int weapon = reader.ReadByte();
					int team = reader.ReadByte();
					Vector3 pos;
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat() - 2.f;
					std::string name = reader.ReadRemainingString();
					// TODO: decode name?
					
					WeaponType wType;
					switch(weapon){
						case 0:
							wType = RIFLE_WEAPON;
							break;
						case 1:
							wType = SMG_WEAPON;
							break;
						case 2:
							wType = SHOTGUN_WEAPON;
							break;
						default:
							SPRaise("Received invalid weapon: %d", weapon);
					}
					
					Player *p = new Player(GetWorld(), pId,
										   wType, team,
										   savedPlayerPos[pId],
										   
										   GetWorld()->GetTeam(team).color);
					p->SetPosition(pos);
					GetWorld()->SetPlayer(pId, p);
					
					World::PlayerPersistent& pers = GetWorld()->GetPlayerPersistent(pId);
					
					if(!name.empty()) // sometimes becomes empty
						pers.name = name;
					
					if(pId == GetWorld()->GetLocalPlayerIndex())
						client->LocalPlayerCreated();
				}
					break;
				case PacketTypeBlockAction:
				{
					Player *p = GetPlayerOrNull(reader.ReadByte());
					int action = reader.ReadByte();
					IntVector3 pos;
					pos.x = reader.ReadInt();
					pos.y = reader.ReadInt();
					pos.z = reader.ReadInt();
					
					std::vector<IntVector3> cells;
					if(action == 0){
						if(!p){
							GetWorld()->CreateBlock(pos, temporaryPlayerBlockColor);
						}else{
							GetWorld()->CreateBlock(pos, p->GetBlockColor());
							client->PlayerCreatedBlock(p);
							p->UsedBlocks(1);
						}
					}else if(action == 1){
						cells.push_back(pos);
						client->PlayerDestroyedBlockWithWeaponOrTool(pos);
						GetWorld()->DestroyBlock(cells);
						
						if(p && p->GetTool() == Player::ToolSpade){
							p->GotBlock();
						}
					}else if(action == 2){
						// dig
						client->PlayerDiggedBlock(pos);
						for(int z = -1; z <= 1; z++)
							cells.push_back(IntVector3::Make(pos.x, pos.y, pos.z + z));
						GetWorld()->DestroyBlock(cells);
					}else if(action == 3){
						// grenade
						client->GrenadeDestroyedBlock(pos);
						for(int x = -1; x <= 1; x++)
							for(int y = -1; y <= 1; y++)
								for(int z = -1; z <= 1; z++)
									cells.push_back(IntVector3::Make(pos.x + x, pos.y + y, pos.z + z));
						GetWorld()->DestroyBlock(cells);
					}
				}
					break;
				case PacketTypeBlockLine:
				{
					Player *p = GetPlayerOrNull(reader.ReadByte());
					IntVector3 pos1, pos2;
					pos1.x = reader.ReadInt();
					pos1.y = reader.ReadInt();
					pos1.z = reader.ReadInt();
					pos2.x = reader.ReadInt();
					pos2.y = reader.ReadInt();
					pos2.z = reader.ReadInt();
					
					IntVector3 col = p ? p->GetBlockColor():
					temporaryPlayerBlockColor;
					std::vector<IntVector3> cells;
					cells = GetWorld()->CubeLine(pos1, pos2, 50);
					
					for(size_t i = 0; i < cells.size(); i++){
						if(!GetWorld()->GetMap()->IsSolid(cells[i].x, cells[i].y, cells[i].z)){
							GetWorld()->CreateBlock(cells[i], col);
							if(p)
							p->UsedBlocks(1);
						}
					}
					
					if(p){
						client->PlayerCreatedBlock(p);
					}
				}
					break;
				case PacketTypeStateData:
					if(!GetWorld())
						break;
				{
					// receives my player info.
					int pId = reader.ReadByte();
					IntVector3 fogColor = reader.ReadIntColor();
					IntVector3 teamColors[2];
					teamColors[0] = reader.ReadIntColor();
					teamColors[1] = reader.ReadIntColor();
					
					std::string teamNames[2];
					teamNames[0] = reader.ReadString(10);
					teamNames[1] = reader.ReadString(10);
					
					World::Team& t1 = GetWorld()->GetTeam(0);
					World::Team& t2 = GetWorld()->GetTeam(1);
					t1.color = teamColors[0];
					t2.color = teamColors[1];
					t1.name = teamNames[0];
					t2.name = teamNames[1];
					
					GetWorld()->SetFogColor(fogColor);
					GetWorld()->SetLocalPlayerIndex(pId);
					
					int mode = reader.ReadByte();
					if(mode == 0){
						// CTF
						CTFGameMode *mode = new CTFGameMode();
						try{
							CTFGameMode::Team& mt1 = mode->GetTeam(0);
							CTFGameMode::Team& mt2 = mode->GetTeam(1);
							
							mt1.score = reader.ReadByte();
							mt2.score = reader.ReadByte();
							mode->SetCaptureLimit(reader.ReadByte());
							
							int intelFlags = reader.ReadByte();
							mt1.hasIntel = (intelFlags & 1);
							mt2.hasIntel = (intelFlags & 2);
							
							if(mt2.hasIntel){
								mt1.carrier = reader.ReadByte();
								reader.ReadData(11);
							}else{
								mt1.flagPos.x = reader.ReadFloat();
								mt1.flagPos.y = reader.ReadFloat();
								mt1.flagPos.z = reader.ReadFloat();
							}
							
							if(mt1.hasIntel){
								mt2.carrier = reader.ReadByte();
								reader.ReadData(11);
							}else{
								mt2.flagPos.x = reader.ReadFloat();
								mt2.flagPos.y = reader.ReadFloat();
								mt2.flagPos.z = reader.ReadFloat();
							}
							
							mt1.basePos.x = reader.ReadFloat();
							mt1.basePos.y = reader.ReadFloat();
							mt1.basePos.z = reader.ReadFloat();
							
							mt2.basePos.x = reader.ReadFloat();
							mt2.basePos.y = reader.ReadFloat();
							mt2.basePos.z = reader.ReadFloat();
							
							GetWorld()->SetMode(mode);
						}catch(...){
							delete mode;
							throw;
						}
					}else{
						// TC
						TCGameMode *mode = new TCGameMode(GetWorld());
						try{
							int numTer = reader.ReadByte();
							
							for(int i = 0; i < numTer; i++){
								TCGameMode::Territory ter;
								ter.pos.x = reader.ReadFloat();
								ter.pos.y = reader.ReadFloat();
								ter.pos.z = reader.ReadFloat();
								
								int state = reader.ReadByte();
								ter.ownerTeamId = state;
								ter.progressBasePos = 0.f;
								ter.progressStartTime = 0.f;
								ter.progressRate = 0.f;
								ter.capturingTeamId = -1;
								ter.mode = mode;
								mode->AddTerritory(ter);
							}
							
							GetWorld()->SetMode(mode);
						}catch(...){
							delete mode;
							throw;
						}
					}
					client->JoinedGame();
				}
					break;
				case PacketTypeKillAction:
				{
					Player *p = GetPlayer(reader.ReadByte());
					Player *killer = GetPlayer(reader.ReadByte());
					int kt = reader.ReadByte();
					KillType type;
					switch(kt){
						case 0: type = KillTypeWeapon; break;
						case 1: type = KillTypeHeadshot; break;
						case 2: type = KillTypeMelee; break;
						case 3: type = KillTypeGrenade; break;
						case 4: type = KillTypeFall; break;
						case 5: type = KillTypeTeamChange; break;
						case 6: type = KillTypeClassChange; break;
						default:
							SPInvalidEnum("kt", kt);
					}
					
					int respawnTime = reader.ReadByte();
					switch(type){
						case KillTypeFall:
						case KillTypeClassChange:
						case KillTypeTeamChange:
							killer = p;
							break;
						default:
							break;
					}
					p->KilledBy(type, killer, respawnTime);
					if(p != killer){
						GetWorld()->GetPlayerPersistent(killer->GetId()).kills += 1;
					}
				}
					break;
				case PacketTypeChatMessage:
				{
					// might be wrong player id for server message
					uint8_t pId = reader.ReadByte();
					Player *p;
					if(pId < 32){
						p = GetPlayer(pId);
					}else{
						p = NULL;
					}
					int type = reader.ReadByte();
					std::string txt = reader.ReadRemainingString();
					if(p){
						switch(type){
							case 0: // all
								client->PlayerSentChatMessage(p, true, txt);
								break;
							case 1: // team
								client->PlayerSentChatMessage(p, false, txt);
								break;
							case 2: // system???
								client->ServerSentMessage(txt);
								/*SPRaise("Player #%d %s sent system message", p->GetId(), p->GetName().c_str());*/
						}
					}else{
						client->ServerSentMessage(txt);
					}
				}
					break;
				case PacketTypeMapStart:
				{
					// next map!
					client->SetWorld(NULL);
					mapSize = reader.ReadInt();
					status = NetClientStatusReceivingMap;
					statusString = "Loading snapshot";
				}
					break;
				case PacketTypeMapChunk:
					SPRaise("Unexpected: received Map Chunk while game");
				case PacketTypePlayerLeft:
				{
					Player *p = GetPlayer(reader.ReadByte());
					GetWorld()->SetPlayer(p->GetId(), NULL);
					// TODO: message
				}
					break;
				case PacketTypeTerritoryCapture:
				{
					int territoryId = reader.ReadByte();
					bool winning = reader.ReadByte() != 0;
					int state = reader.ReadByte();
					
					TCGameMode *mode = dynamic_cast<TCGameMode *>(GetWorld()->GetMode());
					if(!mode) SPRaise("Not TC");
					
					if(territoryId >= mode->GetNumTerritories()){
						SPRaise("Invalid territory id %d specified (max = %d)", territoryId,
								mode->GetNumTerritories()-1);
					}
					
					client->TeamCapturedTerritory(state, territoryId);
					
					TCGameMode::Territory *t = mode->GetTerritory(territoryId);
					
					t->ownerTeamId = state;
					t->progressBasePos = 0.f;
					t->progressRate = 0.f;
					t->progressStartTime = 0.f;
					t->capturingTeamId = -1;
					
					if(winning)
						client->TeamWon(state);
				}
					break;
				case PacketTypeProgressBar:
				{
					int territoryId = reader.ReadByte();
					int capturingTeam = reader.ReadByte();
					int rate = (int8_t)reader.ReadByte();
					float progress = reader.ReadFloat();
					
					TCGameMode *mode = dynamic_cast<TCGameMode *>(GetWorld()->GetMode());
					if(!mode) SPRaise("Not TC");
					
					if(territoryId >= mode->GetNumTerritories()){
						SPRaise("Invalid territory id %d specified (max = %d)", territoryId,
								mode->GetNumTerritories()-1);
					}
					
					if(progress < -0.1f || progress > 1.1f)
						SPRaise("Progress value out of range(%f)", progress);
					
					TCGameMode::Territory *t = mode->GetTerritory(territoryId);
					
					t->progressBasePos = progress;
					t->progressRate = (float)rate * TC_CAPTURE_RATE;
					t->progressStartTime = GetWorld()->GetTime();
					t->capturingTeamId = capturingTeam;
				}
					break;
				case PacketTypeIntelCapture:
				{
					if(!GetWorld()) SPRaise("No world");
					CTFGameMode *mode = dynamic_cast<CTFGameMode *>(GetWorld()->GetMode());
					if(!mode) SPRaise("Not CTF");
					Player *p = GetPlayer(reader.ReadByte());
					client->PlayerCapturedIntel(p);
					GetWorld()->GetPlayerPersistent(p->GetId()).kills += 10;
					mode->GetTeam(p->GetTeamId()).hasIntel = false;
					mode->GetTeam(p->GetTeamId()).score++;
					
					bool winning = reader.ReadByte() != 0;
					if(winning)
						client->TeamWon(p->GetTeamId());
				}
					break;
				case PacketTypeIntelPickup:
				{
					Player *p = GetPlayer(reader.ReadByte());
					CTFGameMode *mode = dynamic_cast<CTFGameMode *>(GetWorld()->GetMode());
					if(!mode) SPRaise("Not CTF");
					CTFGameMode::Team& team = mode->GetTeam(p->GetTeamId());
					team.hasIntel = true;
					team.carrier = p->GetId();
					client->PlayerPickedIntel(p);
				}
					break;
				case PacketTypeIntelDrop:
				{
					Player *p = GetPlayer(reader.ReadByte());
					CTFGameMode *mode = dynamic_cast<CTFGameMode *>(GetWorld()->GetMode());
					if(!mode) SPRaise("Not CTF");
					CTFGameMode::Team& team = mode->GetTeam(p->GetTeamId());
					team.hasIntel = false;
					
					Vector3 pos;
					pos.x = reader.ReadFloat();
					pos.y = reader.ReadFloat();
					pos.z = reader.ReadFloat();
					
					mode->GetTeam(1 - p->GetTeamId()).flagPos = pos;
					
					client->PlayerDropIntel(p);
				}
					break;
				case PacketTypeRestock:
				{
					Player *p = GetLocalPlayer();//GetPlayer(reader.ReadByte());
					p->Restock();
				}
					break;
				case PacketTypeFogColour:
				{
					if(GetWorld()){
						reader.ReadByte(); // skip
						GetWorld()->SetFogColor(reader.ReadIntColor());
					}
				}
					break;
				case PacketTypeWeaponReload:
				{
					Player *p = GetPlayer(reader.ReadByte());
					if(p != GetLocalPlayerOrNull())
						p->Reload();
					
					// FIXME: use of "clip ammo" and "reserve ammo"?
				}
					break;
				case PacketTypeChangeTeam:
				{
					Player *p = GetPlayer(reader.ReadByte());
					int team = reader.ReadByte();
					if(team < 0 || team > 2)
						SPRaise("Received invalid team: %d", team);
					p->SetTeam(team);
				}
				case PacketTypeChangeWeapon:
				{
					Player * p = GetPlayerOrNull(reader.ReadByte());
					WeaponType wType;
					int weapon = reader.ReadByte();
					switch(weapon){
						case 0:
							wType = RIFLE_WEAPON;
							break;
						case 1:
							wType = SMG_WEAPON;
							break;
						case 2:
							wType = SHOTGUN_WEAPON;
							break;
						default:
							SPRaise("Received invalid weapon: %d", weapon);
					}
					// maybe this command is intended to change local player's
					// weapon...
					//p->SetWeaponType(wType);
				}
					break;
				default:
					printf("WARNING: dropped packet %d\n", (int)reader.GetType());
					reader.DumpDebug();
			}
		}
		
		void NetClient::SendJoin(int team, WeaponType weapType, std::string name, int kills){
			SPADES_MARK_FUNCTION();
			int weapId;
			switch(weapType) {
				case RIFLE_WEAPON: weapId = 0; break;
				case SMG_WEAPON: weapId = 1; break;
				case SHOTGUN_WEAPON: weapId = 2; break;
				default: SPInvalidEnum("weapType", weapType);
			}
			
			NetPacketWriter wri(PacketTypeExistingPlayer);
			wri.Write((uint8_t)GetWorld()->GetLocalPlayerIndex());
			wri.Write((uint8_t)team);
			wri.Write((uint8_t)weapId);
			wri.Write((uint8_t)2); // TODO: change tool
			wri.Write((uint32_t)kills);
			wri.WriteColor(GetWorld()->GetTeam(team).color);
			wri.Write(name, 16);
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendPosition(){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypePositionData);
			//wri.Write((uint8_t)pId);
			Player *p = GetLocalPlayer();
			Vector3 v = p->GetPosition();
			wri.Write(v.x);
			wri.Write(v.y);
			wri.Write(v.z);
			enet_peer_send(peer, 0, wri.CreatePacket());
			//printf("> (%f %f %f)\n", v.x, v.y, v.z);
		}
		
		void NetClient::SendOrientation(spades::Vector3 v){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeOrientationData);
			//wri.Write((uint8_t)pId);
			wri.Write(v.x);
			wri.Write(v.y);
			wri.Write(v.z);
			enet_peer_send(peer, 0, wri.CreatePacket());
			//printf("> (%f %f %f)\n", v.x, v.y, v.z);
		}
		
		void NetClient::SendPlayerInput(PlayerInput inp) {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeInputData);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			uint8_t bits = 0;
			if(inp.moveForward)		bits |= 1 << 0;
			if(inp.moveBackward)	bits |= 1 << 1;
			if(inp.moveLeft)		bits |= 1 << 2;
			if(inp.moveRight)		bits |= 1 << 3;
			if(inp.jump)		bits |= 1 << 4;
			if(inp.crouch)		bits |= 1 << 5;
			if(inp.sneak)		bits |= 1 << 6;
			if(inp.sprint)		bits |= 1 << 7;
			wri.Write(bits);
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendWeaponInput( WeaponInput inp) {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeWeaponInput);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			uint8_t bits = 0;
			if(inp.primary)		bits |= 1 << 0;
			if(inp.secondary)	bits |= 1 << 1;
			wri.Write(bits);
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendBlockAction(spades::IntVector3 v,
										BlockActionType type){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeBlockAction);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			switch(type){
				case BlockActionCreate: wri.Write((uint8_t)0); break;
				case BlockActionTool: wri.Write((uint8_t)1); break;
				case BlockActionDig: wri.Write((uint8_t)2); break;
				case BlockActionGrenade: wri.Write((uint8_t)3); break;
				default: SPInvalidEnum("type", type);
			}
			
			wri.Write((uint32_t)v.x);
			wri.Write((uint32_t)v.y);
			wri.Write((uint32_t)v.z);
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendBlockLine(spades::IntVector3 v1,
									  spades::IntVector3 v2) {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeBlockLine);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			wri.Write((uint32_t)v1.x);
			wri.Write((uint32_t)v1.y);
			wri.Write((uint32_t)v1.z);
			wri.Write((uint32_t)v2.x);
			wri.Write((uint32_t)v2.y);
			wri.Write((uint32_t)v2.z);
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendReload() {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeWeaponReload);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			wri.Write((uint8_t)0); // clip_ammo; not used?
			wri.Write((uint8_t)0); // reserve_ammo; not used?
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendHeldBlockColor() {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeSetColour);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			IntVector3 v = GetLocalPlayer()->GetBlockColor();
			wri.WriteColor(v);
			enet_peer_send(peer, 0, wri.CreatePacket());
			
		}
		
		void NetClient::SendTool(){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeSetTool);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			switch(GetLocalPlayer()->GetTool()){
				case Player::ToolSpade:
					wri.Write((uint8_t)0); break;
				case Player::ToolBlock:
					wri.Write((uint8_t)1); break;
				case Player::ToolWeapon:
					wri.Write((uint8_t)2); break;
				case Player::ToolGrenade:
					wri.Write((uint8_t)3); break;
				default:
					SPInvalidEnum("tool", GetLocalPlayer()->GetTool());
			}
			
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendGrenade(spades::client::Grenade *g){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeGrenadePacket);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			
			wri.Write(g->GetFuse());
			
			Vector3 v = g->GetPosition();
			wri.Write(v.x);
			wri.Write(v.y);
			wri.Write(v.z);
			
			v = g->GetVelocity();
			wri.Write(v.x);
			wri.Write(v.y);
			wri.Write(v.z);
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendHit(int targetPlayerId, HitType type){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeHitPacket);
			wri.Write((uint8_t)targetPlayerId);
			
			switch(type){
				case HitTypeTorso:
					wri.Write((uint8_t)0);
					break;
				case HitTypeHead:
					wri.Write((uint8_t)1);
					break;
				case HitTypeArms:
					wri.Write((uint8_t)2);
					break;
				case HitTypeLegs:
					wri.Write((uint8_t)3);
					break;
				case HitTypeMelee:
					wri.Write((uint8_t)4);
					break;
				default:
					SPInvalidEnum("type", type);
			}
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendChat(std::string text,
								 bool global) {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeChatMessage);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			wri.Write((uint8_t)(global?0:1));
			wri.Write(text);
			wri.Write((uint8_t)0);
			enet_peer_send(peer, 0, wri.CreatePacket());
		}
		
		void NetClient::SendWeaponChange(WeaponType wt){
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeChangeWeapon);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			switch(wt){
				case RIFLE_WEAPON:
					wri.Write((uint8_t)0);
					break;
				case SMG_WEAPON:
					wri.Write((uint8_t)1);
					break;
				case SHOTGUN_WEAPON:
					wri.Write((uint8_t)2);
					break;
			}
			enet_peer_send(peer, 0, wri.CreatePacket());
			
		}
		
		void NetClient::SendTeamChange(int team) {
			SPADES_MARK_FUNCTION();
			NetPacketWriter wri(PacketTypeChangeTeam);
			wri.Write((uint8_t)GetLocalPlayer()->GetId());
			wri.Write((uint8_t)team);
			enet_peer_send(peer, 0, wri.CreatePacket());
			
		}
		
		void NetClient::MapLoaded() {
			SPADES_MARK_FUNCTION();
			MemoryStream compressed(mapData.data(),
									mapData.size());
			DeflateStream inflate(&compressed, CompressModeDecompress, false);
			GameMap *map;
			map = GameMap::Load(&inflate);
			
			// now initialize world
			World *w = new World();
			w->SetMap(map);
			client->SetWorld(w);
			
			mapData.clear();
			
			SPAssert(GetWorld());
			
			// do saved packets
			try{
				for(size_t i =0;i<savedPackets.size();i++){
					NetPacketReader r(savedPackets[i]);
					Handle(r);
				}
				savedPackets.clear();
			}catch(...){
				savedPackets.clear();
				throw;
			}
		}
		
	}
}