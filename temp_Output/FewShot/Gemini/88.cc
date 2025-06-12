#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"

using namespace ns3;

static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_UNCOND("Remaining Energy = " << remainingEnergy << " J");
}

static void TotalEnergyConsumed(double oldValue, double totalEnergyConsumed) {
    NS_LOG_UNCOND("Total Energy Consumed = " << totalEnergyConsumed << " J");
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1024;
    Time simStartTime = Seconds(0.0);
    double distance = 10.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("packetSize", "Size of packets sent", packetSize);
    cmd.AddValue("startTime", "Simulation start time (seconds)", simStartTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
    address.Assign(staDevices);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(simStartTime);
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0) + simStartTime);
    clientApps.Stop(Seconds(10.0));

    BasicEnergySourceHelper basicSourceHelper;
    EnergySourceContainer sources = basicSourceHelper.Install(nodes.Get(0));
    WifiRadioEnergyModelHelper radioEnergyModelHelper;
    radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.0194));
    radioEnergyModelHelper.Set("IdleCurrentA", DoubleValue(0.0011));
    radioEnergyModelHelper.Set("SleepCurrentA", DoubleValue(0.00008));
    EnergySourceContainer deviceModels = radioEnergyModelHelper.Install(staDevices.Get(0), sources.Get(0));

     BasicEnergySourceHelper basicSourceHelper2;
    EnergySourceContainer sources2 = basicSourceHelper2.Install(nodes.Get(1));
    WifiRadioEnergyModelHelper radioEnergyModelHelper2;
    radioEnergyModelHelper2.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyModelHelper2.Set("RxCurrentA", DoubleValue(0.0194));
    radioEnergyModelHelper2.Set("IdleCurrentA", DoubleValue(0.0011));
    radioEnergyModelHelper2.Set("SleepCurrentA", DoubleValue(0.00008));
    EnergySourceContainer deviceModels2 = radioEnergyModelHelper2.Install(apDevices.Get(0), sources2.Get(0));

    Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource>(sources.Get(0));
    energySource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
    energySource->TraceConnectWithoutContext("TotalEnergyConsumed", MakeCallback(&TotalEnergyConsumed));

    Ptr<BasicEnergySource> energySource2 = DynamicCast<BasicEnergySource>(sources2.Get(0));
    energySource2->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
    energySource2->TraceConnectWithoutContext("TotalEnergyConsumed", MakeCallback(&TotalEnergyConsumed));

    Simulator::Stop(Seconds(10.0 + simStartTime.GetSeconds()));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}