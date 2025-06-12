#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParameterSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double slotTime = 9;
    double sifsTime = 16;
    double pifsTime = 25;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
    cmd.AddValue("sifsTime", "SIFS time in microseconds", sifsTime);
    cmd.AddValue("pifsTime", "PIFS time in microseconds", pifsTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    Ssid ssid = Ssid("wifi-timing-example");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    Ptr<WifiNetDevice> apWifiNetDevice = DynamicCast<WifiNetDevice>(apDevices.Get(0));
    Ptr<WifiMac> apMac = apWifiNetDevice->GetMac();
    Ptr<EdcaParameterSet> edca = apMac->GetObject<EdcaParameterSet>();
    if (edca) {
        edca->SetAttribute("Slot", TimeValue(MicroSeconds(slotTime)));
        edca->SetAttribute("Sifs", TimeValue(MicroSeconds(sifsTime)));
        edca->SetAttribute("Pifs", TimeValue(MicroSeconds(pifsTime)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    address.NewNetwork();
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps = client.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalRx = server.GetServer()->GetReceived();
    double throughput = totalRx * 8 / (simulationTime * 1000000.0);

    NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");

    Simulator::Destroy();

    return 0;
}