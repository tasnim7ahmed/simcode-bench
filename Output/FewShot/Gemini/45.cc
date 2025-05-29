#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string phyMode("DsssRate1Mbps");
    double primaryRss = -80;
    double interferingRss = -70;
    double timeOffset = 0.000001; // 1 us
    uint32_t packetSizePrimary = 1000;
    uint32_t packetSizeInterfering = 500;
    bool tracing = false;
    std::string animFile = "wifi-interference.xml";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("primaryRss", "Received signal strength (dBm) of primary signal", primaryRss);
    cmd.AddValue("interferingRss", "Received signal strength (dBm) of interfering signal", interferingRss);
    cmd.AddValue("timeOffset", "Time offset between primary and interfering signals (seconds)", timeOffset);
    cmd.AddValue("packetSizePrimary", "Size of primary packet", packetSizePrimary);
    cmd.AddValue("packetSizeInterfering", "Size of interfering packet", packetSizeInterfering);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("animFile", "NetAnim animation file", animFile);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("wifi-interference")));

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpClientHelper clientHelper(interfaces.GetAddress(1), 9);
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSizePrimary));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    UdpClientHelper interferingClientHelper(interfaces.GetAddress(1), 9);
    interferingClientHelper.SetAttribute("PacketSize", UintegerValue(packetSizeInterfering));
    interferingClientHelper.SetAttribute("MaxPackets", UintegerValue(1));
    interferingClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));

    ApplicationContainer interferingClientApp = interferingClientHelper.Install(nodes.Get(2));
    interferingClientApp.Start(Seconds(1.0 + timeOffset));
    interferingClientApp.Stop(Seconds(10.0));
    
    UdpServerHelper serverHelper(9);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    if (tracing) {
        wifiPhy.EnablePcapAll("wifi-interference");
    }

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(nodes.Get(0), 0, 10);
    anim.SetConstantPosition(nodes.Get(1), 10, 10);
    anim.SetConstantPosition(nodes.Get(2), 5, 15);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}