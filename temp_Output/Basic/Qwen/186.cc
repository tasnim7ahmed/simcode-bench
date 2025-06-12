#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/position-allocator.h"
#include "ns3/wave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETSimulation");

int main(int argc, char *argv[]) {
    uint32_t nodeCount = 5;
    double simTime = 10.0;
    double updateInterval = 1.0;

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));
    Config::SetDefault("ns3::WaveMacHelper::DefaultCwMin", UintegerValue(15));
    Config::SetDefault("ns3::WaveMacHelper::DefaultCwMax", UintegerValue(1023));

    NodeContainer nodes;
    nodes.Create(nodeCount);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    Wifi80211pMacHelper wifiMac = Wifi80211pMacHelper();
    WifiHelper wifi = WifiHelper();
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it) {
        Ptr<Node> node = (*it);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector position = mobility->GetPosition();
        Vector velocity = Vector(20.0, 0.0, 0.0); // All vehicles move east at 20 m/s
        mobility->SetPosition(position);
        mobility->SetVelocity(velocity);
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(200));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));

    for (uint32_t i = 0; i < nodeCount; ++i) {
        for (uint32_t j = 0; j < nodeCount; ++j) {
            if (i != j) {
                uint16_t port = 8000 + j;
                Address sinkAddress(InetSocketAddress(interfaces.GetAddress(j), port));
                ApplicationContainer apps = onoff.Install(nodes.Get(i));
                apps.Start(Seconds(0.0));
                apps.Stop(Seconds(simTime));
                PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps = packetSink.Install(nodes.Get(j));
                apps.Start(Seconds(0.0));
                apps.Stop(Seconds(simTime));
            }
        }
    }

    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));
    for (uint32_t i = 0; i < nodeCount; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Vehicle-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}