#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/applications-module.h"

using namespace ns3;

void MoveAp(Ptr<Node> apNode, Ptr<ConstantPositionMobilityModel> apMobility, double step, double duration, double maxTime, double &movedTime)
{
    Vector pos = apMobility->GetPosition();
    pos.x += step;
    apMobility->SetPosition(pos);
    movedTime += duration;
    if (movedTime < maxTime)
    {
        Simulator::Schedule(Seconds(duration), &MoveAp, apNode, apMobility, step, duration, maxTime, std::ref(movedTime));
    }
}

void MacTxCallback(std::string context, Ptr<const Packet> pkt)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] " << context << " - MAC TX " << pkt->GetSize() << " bytes");
}

void MacRxCallback(std::string context, Ptr<const Packet> pkt)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] " << context << " - MAC RX " << pkt->GetSize() << " bytes");
}

void PhyTxBeginCallback(std::string context, Ptr<const Packet> pkt)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] " << context << " - PHY TX BEGIN " << pkt->GetSize() << " bytes");
}

void PhyRxOkCallback(std::string context, Ptr<const Packet> pkt, double, WifiMode, enum WifiPreamble)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] " << context << " - PHY RX OK " << pkt->GetSize() << " bytes");
}

void PhyRxErrorCallback(std::string context, Ptr<const Packet> pkt, double)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] " << context << " - PHY RX ERROR " << pkt->GetSize() << " bytes");
}

void StateChangeCallback(std::string context, Time start, Time duration, WifiPhy::State oldState, WifiPhy::State newState)
{
    NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "s] "
                    << context << " - PHY STATE " << WifiPhy::StateToString(oldState)
                    << " -> " << WifiPhy::StateToString(newState));
}

int main(int argc, char* argv[])
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    uint32_t numStas = 2;
    double simulationTime = 44.0;
    double commStart = 0.5;

    // Create nodes: 2 stations + 1 AP
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(numStas);
    wifiApNode.Create(1);
    NodeContainer allNodes;
    allNodes.Add(wifiStaNodes);
    allNodes.Add(wifiApNode);

    // Install Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("sim-ssid");

    // Stations
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Access Point
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set data rate on all devices
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        staDevices.Get(i)->SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    }
    apDevice.Get(0)->SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));

    // Mobility
    MobilityHelper mobilitySta, mobilityAp;
    // For stations - static positions
    mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(5.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(2),
        "LayoutType", StringValue("RowFirst"));
    mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilitySta.Install(wifiStaNodes);
    // For ap - movable, will change x by 5 m every sec
    mobilityAp.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(0.0),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(1),
        "LayoutType", StringValue("RowFirst"));
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Packet socket install
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    // Attach traces for staDevices
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        std::ostringstream ctx;
        ctx << "/NodeList/" << wifiStaNodes.Get(i)->GetId() << "/DeviceList/0";
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Mac/MacTx", MakeBoundCallback(&MacTxCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Mac/MacRx", MakeBoundCallback(&MacRxCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeBoundCallback(&PhyRxOkCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyRxError", MakeBoundCallback(&PhyRxErrorCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/State", MakeBoundCallback(&StateChangeCallback, ctx.str()));
    }
    // Access Point traces
    {
        std::ostringstream ctx;
        ctx << "/NodeList/" << wifiApNode.Get(0)->GetId() << "/DeviceList/0";
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Mac/MacTx", MakeBoundCallback(&MacTxCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Mac/MacRx", MakeBoundCallback(&MacRxCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeBoundCallback(&PhyRxOkCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/PhyRxError", MakeBoundCallback(&PhyRxErrorCallback, ctx.str()));
        Config::Connect(ctx.str() + "/$ns3::WifiNetDevice/Phy/State", MakeBoundCallback(&StateChangeCallback, ctx.str()));
    }

    // Create a packet sink socket on STA 1
    PacketSocketAddress sinkAddr;
    sinkAddr.SetSingleDevice(staDevices.Get(1)->GetIfIndex());
    sinkAddr.SetPhysicalAddress(staDevices.Get(1)->GetAddress());
    sinkAddr.SetProtocol(1);

    Ptr<PacketSink> pktSink = CreateObject<PacketSink>();
    pktSink->SetAttribute("Protocol", UintegerValue(1));
    Ptr<Node> staNode1 = wifiStaNodes.Get(1);
    staNode1->AddApplication(pktSink);
    pktSink->SetStartTime(Seconds(commStart));
    pktSink->SetStopTime(Seconds(simulationTime));

    // Create an OnOff application on STA 0 to send to STA 1 (using packet socket)
    Ptr<OnOffApplication> onoff = CreateObject<OnOffApplication>();
    onoff->SetAttribute("Remote", AddressValue(sinkAddr));
    onoff->SetAttribute("Protocol", UintegerValue(1));
    onoff->SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    onoff->SetAttribute("PacketSize", UintegerValue(512));
    onoff->SetAttribute("StartTime", TimeValue(Seconds(commStart)));
    onoff->SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    wifiStaNodes.Get(0)->AddApplication(onoff);

    // Move AP every second by 5 meters along X
    Ptr<MobilityModel> apMobilityModel = wifiApNode.Get(0)->GetObject<MobilityModel>();
    Ptr<ConstantPositionMobilityModel> apMobility = DynamicCast<ConstantPositionMobilityModel>(apMobilityModel);
    double movedTime = 0.0;
    Simulator::Schedule(Seconds(1.0), &MoveAp, wifiApNode.Get(0), apMobility, 5.0, 1.0, simulationTime, std::ref(movedTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}