#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETSimulation");

int main(int argc, char *argv[]) {
    uint32_t nVehicles = 10;
    double simTime = 80.0;
    std::string phyMode("DsssRate1Mbps");
    std::string traceFile = "vanet-mobility.trace";
    std::string animFile = "vanet-animation.xml";

    // Enable DSR
    Config::SetDefault("ns3::DsrRoutingHelper::PassiveAckTimeout", TimeValue(Seconds(5.0)));
    Config::SetDefault("ns3::DsrRoutingHelper::RequestTableSize", UintegerValue(64));
    Config::SetDefault("ns3::DsrRoutingHelper::MaxSendBuffLen", UintegerValue(128));
    Config::SetDefault("ns3::DsrRoutingHelper::MaxSendBufferSize", UintegerValue(65536));

    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());
    wifiMac.SetType("ns3::AdhocWifiMac");
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);
    for (NodeContainer::Iterator i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
        Vector position = Vector(rand() % 1000, rand() % 1000, 0.0);
        model->SetPosition(position);
        Vector velocity = Vector((rand() % 10) + 5, (rand() % 10) + 5, 0.0);
        model->SetVelocity(velocity);
    }

    InternetStackHelper stack;
    DsrRoutingHelper dsrRouting;
    stack.SetRoutingHelper(dsrRouting);
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps = onoff.Install(vehicles.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(vehicles.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simTime));

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet-wifi.tr"));

    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle-" + std::to_string(i));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("vanet-flow.xml", false, false);

    Simulator::Destroy();
    return 0;
}