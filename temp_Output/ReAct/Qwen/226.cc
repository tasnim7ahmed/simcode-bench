#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkOLSR");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double simulationTime = 50.0;
    double dataInterval = 5.0;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of sensor nodes (including sink)", numNodes);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("dataInterval", "Interval between UDP sends in seconds", dataInterval);
    cmd.Parse(argc, argv);

    numNodes = std::max(numNodes, (uint32_t)2); // At least sink and one sensor

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    ApplicationContainer serverApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        InetSocketAddress sinkLocalAddress(interfaces.GetAddress(0), 8000);
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
        serverApps.Add(packetSinkHelper.Install(nodes.Get(0)));
    }

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        InetSocketAddress sinkRemoteAddress(interfaces.GetAddress(0), 8000);
        OnOffHelper onoff("ns3::UdpSocketFactory", sinkRemoteAddress);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("64kbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer app = onoff.Install(nodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime - 1.0));
        clientApps.Add(app);
    }

    EnergySourceHelper energySourceHelper;
    energySourceHelper.Set("EnergySourceInitialVoltage", DoubleValue(3.0));
    energySourceHelper.Set("BatteryCapacity", DoubleValue(20000)); // mAh
    energySourceHelper.Set("VoltageSag", BooleanValue(false));
    energySourceHelper.Set("InternalResistance", DoubleValue(0.0));
    energySourceHelper.Set("NominalVoltage", DoubleValue(3.0));
    energySourceHelper.Set("CutoffVoltage", DoubleValue(1.0));

    BasicEnergySourceContainer sources = energySourceHelper.Install(nodes);

    DeviceEnergyModelHelper deviceEnergyModelHelper;
    deviceEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.017)); // 17 mA
    deviceEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.019)); // 19 mA
    deviceEnergyModelHelper.Set("IdleCurrentA", DoubleValue(0.002)); // 2 mA
    deviceEnergyModelHelper.Set("SleepCurrentA", DoubleValue(0.000001)); // 1 uA

    deviceEnergyModelHelper.Install(devices, sources);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}