#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sumo-file-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_UDP_DSR_SUMO");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer nodes;
    nodes.Create(10); // Adjust number of vehicles as needed

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    wifi.SetChannel(wifiChannel);

    WifiMacHelper mac;
    Ssid ssid = Ssid("vanet-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer devices = wifi.Install(nodes);

    // Mobility using SUMO trace file
    SumoFileHelper sumoHelper;
    sumoHelper.SetMobilityModel();
    sumoHelper.Install(nodes, "vanet-trace.sumocfg"); // Ensure this file exists in working directory

    // Internet stack with DSR routing
    InternetStackHelper stack;
    DsrRoutingHelper dsrRouting;
    stack.SetRoutingHelper(dsrRouting);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install applications
    uint16_t port = 9; // Discard port

    // Server node (node 0)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(80.0));

    // Client node (node 1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(80.0));

    // Animation output
    AnimationInterface anim("vanet_udp_dsr_sumo.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(80.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}