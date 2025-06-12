#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpSimulation");

int main(int argc, char *argv[]) {
    uint32_t numStationsPerAp = 5;
    double simulationTime = 10.0;
    bool enablePcap = true;
    bool enableAscii = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStationsPerAp", "Number of stations per access point", numStationsPerAp);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("pcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("ascii", "Enable ASCII tracing", enableAscii);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes[2];
    for (uint32_t i = 0; i < 2; ++i) {
        staNodes[i].Create(numStationsPerAp);
    }

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    NetDeviceContainer apDevices[2], staDevices[2];

    Ssid ssid1 = Ssid("wifi-network-1");
    Ssid ssid2 = Ssid("wifi-network-2");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices[0] = wifi.Install(phy, mac, staNodes[0]);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    apDevices[0] = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices[1] = wifi.Install(phy, mac, staNodes[1]);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    apDevices[1] = wifi.Install(phy, mac, apNodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    for (uint32_t i = 0; i < 2; ++i) {
        mobility.Install(staNodes[i]);
    }

    InternetStackHelper stack;
    stack.Install(apNodes);
    for (uint32_t i = 0; i < 2; ++i) {
        stack.Install(staNodes[i]);
    }

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[2], staInterfaces[2];

    address.SetBase("192.168.1.0", "255.255.255.0");
    apInterfaces[0] = address.Assign(apDevices[0]);
    staInterfaces[0] = address.Assign(staDevices[0]);

    address.SetBase("192.168.2.0", "255.255.255.0");
    apInterfaces[1] = address.Assign(apDevices[1]);
    staInterfaces[1] = address.Assign(staDevices[1]);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer csmaNodes;
    csmaNodes.Add(apNodes);
    csmaNodes.Create(1); // Common server node

    NetDeviceContainer p2pDevices[2];
    p2pDevices[0] = p2p.Install(apNodes.Get(0), csmaNodes.Get(2));
    p2pDevices[1] = p2p.Install(apNodes.Get(1), csmaNodes.Get(2));

    Ipv4InterfaceContainer p2pInterfaces[2];
    address.SetBase("10.1.1.0", "255.255.255.0");
    p2pInterfaces[0] = address.Assign(p2pDevices[0]);
    address.NewNetwork();
    p2pInterfaces[1] = address.Assign(p2pDevices[1]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(csmaNodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 2; ++i) {
        for (uint32_t j = 0; j < numStationsPerAp; ++j) {
            AddressValue remoteAddress(InetSocketAddress(p2pInterfaces[i].GetAddress(1), port));
            onoff.SetAttribute("Remote", remoteAddress);
            clientApps = onoff.Install(staNodes[i].Get(j));
            clientApps.Start(Seconds(1.0));
            clientApps.Stop(Seconds(simulationTime));
        }
    }

    if (enablePcap) {
        phy.EnablePcapAll("wifi_udp_simulation");
    }

    if (enableAscii) {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll(ascii.CreateFileStream("wifi_udp_simulation.tr"));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalRx = StaticCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
    NS_LOG_UNCOND("Total received data: " << totalRx << " bytes");

    for (uint32_t i = 0; i < 2; ++i) {
        for (uint32_t j = 0; j < numStationsPerAp; ++j) {
            Ptr<OnOffApplication> client = DynamicCast<OnOffApplication>(clientApps.Get(i * numStationsPerAp + j));
            uint64_t tx = client->GetTotalTx();
            NS_LOG_UNCOND("Station " << i << "-" << j << " sent: " << tx << " bytes");
        }
    }

    Simulator::Destroy();
    return 0;
}