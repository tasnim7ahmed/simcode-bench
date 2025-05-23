#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMobilitySimulation");

void MoveAp(Ptr<Node> apNode)
{
    Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
    Vector currentPos = apMobility->GetPosition();
    currentPos.x += 5.0;
    apMobility->SetPosition(currentPos);
    if (Simulator::Now().GetSeconds() < 43.0) 
    {
        Simulator::Schedule(Seconds(1.0), &MoveAp, apNode);
    }
}

int main(int argc, char* argv[])
{
    LogComponentEnable("WifiMobilitySimulation", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("my-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpIf = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staIpIfs = ipv4.Assign(staDevices);

    PacketSocketHelper psHelper;
    psHelper.Install(apNode);
    psHelper.Install(staNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    positionAlloc->Add(Vector(15.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    Simulator::Schedule(Seconds(1.0), &MoveAp, apNode.Get(0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(44.0));

    uint32_t packetSize = 500; 
    double dataRateKbps = 500.0;
    Time packetInterval = MicroSeconds(packetSize * 8.0 / dataRateKbps * 1000.0);

    UdpEchoClientHelper echoClient1(apIpIf.GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(100000));
    echoClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    echoClient1.SetAttribute("Interval", TimeValue(packetInterval));
    ApplicationContainer clientApps1 = echoClient1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(0.5));
    clientApps1.Stop(Seconds(44.0));

    UdpEchoClientHelper echoClient2(apIpIf.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(100000));
    echoClient2.SetAttribute("PacketSize", UintegerValue(packetSize));
    echoClient2.SetAttribute("Interval", TimeValue(packetInterval));
    ApplicationContainer clientApps2 = echoClient2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(0.5));
    clientApps2.Stop(Seconds(44.0));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-mobility-phy-trace.tr"));
    mac.EnableAsciiAll(ascii.CreateFileStream("wifi-mobility-mac-trace.tr"));
    
    phy.EnablePcapAll("wifi-mobility-pcap", false);

    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}