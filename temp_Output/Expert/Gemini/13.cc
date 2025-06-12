#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numBackboneRouters = 3;
    uint32_t numLanNodes = 2;
    uint32_t numWifiSta = 2;
    bool enablePcapTracing = false;
    bool enableNetAnim = false;
    bool enableFlowMonitor = false;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("numBackboneRouters", "Number of backbone routers", numBackboneRouters);
    cmd.AddValue("numLanNodes", "Number of LAN nodes per router", numLanNodes);
    cmd.AddValue("numWifiSta", "Number of WiFi STAs per router", numWifiSta);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("enableNetAnim", "Enable NetAnim animation", enableNetAnim);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("ns3-wifi")));

    NodeContainer backboneRouters;
    backboneRouters.Create(numBackboneRouters);

    OlsrHelper olsrRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsrRouting);
    internet.Install(backboneRouters);

    NodeContainer wifiApNodes;
    wifiApNodes.Create(numBackboneRouters);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numBackboneRouters * numWifiSta);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    NetDeviceContainer apDevices;
    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi")));

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        apDevices.Add(wifi.Install(phy, mac, wifiApNodes.Get(i)));
    }

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac");
    for (uint32_t i = 0; i < numBackboneRouters * numWifiSta; ++i) {
        staDevices.Add(wifi.Install(phy, mac, wifiStaNodes.Get(i)));
    }

    InternetStackHelper wifiInternet;
    wifiInternet.Install(wifiApNodes);
    wifiInternet.Install(wifiStaNodes);

    NodeContainer lanNodes;
    lanNodes.Create(numBackboneRouters * numLanNodes);

    InternetStackHelper lanInternet;
    lanInternet.Install(lanNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        NodeContainer lanSegment;
        lanSegment.Add(backboneRouters.Get(i));
        for (uint32_t j = 0; j < numLanNodes; ++j) {
            lanSegment.Add(lanNodes.Get(i * numLanNodes + j));
        }
        lanDevices[i] = csma.Install(lanSegment);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("100.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(backboneRouters);
    mobility.Install(wifiApNodes);

    MobilityHelper staMobility;
    staMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    staMobility.Install(wifiStaNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer backboneInterfaces = ipv4.Assign(backboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        ipv4.SetBase("10.1." + std::to_string(i + 2) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer lanInterfaces = ipv4.Assign(lanDevices[i]);
    }

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        ipv4.SetBase("10.2." + std::to_string(i + 1) + ".0", "255.255.255.0");
        NetDeviceContainer tempContainer;
        tempContainer.Add(apDevices.Get(i));
        for(uint32_t j = 0; j < numWifiSta; ++j){
            tempContainer.Add(staDevices.Get(i*numWifiSta + j));
        }
        Ipv4InterfaceContainer wifiInterfaces = ipv4.Assign(tempContainer);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpClientHelper client(wifiStaNodes.Get((numBackboneRouters * numWifiSta) - 1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("MaxPackets", UintegerValue(10));

    ApplicationContainer clientApps = client.Install(lanNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNodes.Get((numBackboneRouters * numWifiSta) - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    if (enablePcapTracing) {
        phy.EnablePcapAll("mixed_network");
        csma.EnablePcapAll("mixed_network", false);
    }

    if (enableNetAnim) {
        AnimationInterface anim("mixed_network.xml");
        anim.SetConstantPosition(backboneRouters.Get(0), 10, 10);
        anim.SetConstantPosition(wifiApNodes.Get(0), 20, 10);
    }

    FlowMonitorHelper flowmon;
    if (enableFlowMonitor) {
        flowmon.InstallAll();
    }

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("mixed_network_flowmon.xml", false, true);
    }

    Simulator::Destroy();
    return 0;
}