#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNCSMASimulation");

int main(int argc, char *argv[]) {
    uint32_t numSensorNodes = 5;
    uint32_t numBaseStations = 1;
    double simulationTime = 10.0;
    std::string dataRate = "10kbps";
    uint32_t packetSize = 1024;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numSensorNodes", "Number of sensor nodes", numSensorNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer sensors;
    sensors.Create(numSensorNodes);

    NodeContainer baseStations;
    baseStations.Create(numBaseStations);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer sensorDevices;
    sensorDevices = wifi.Install(phy, mac, sensors);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer baseStationDevices;
    baseStationDevices = wifi.Install(phy, mac, baseStations);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensors);
    mobility.Install(baseStations);

    InternetStackHelper stack;
    stack.Install(sensors);
    stack.Install(baseStations);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer sensorInterfaces;
    sensorInterfaces = address.Assign(sensorDevices);

    Ipv4InterfaceContainer baseStationInterfaces;
    baseStationInterfaces = address.Assign(baseStationDevices);

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(baseStations.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < sensors.GetN(); ++i) {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(baseStationInterfaces.GetAddress(0), port)));
        apps = onoff.Install(sensors.Get(i));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(simulationTime - 0.1));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("wsn-csma.xml");
    anim.SetConstantPosition(baseStations.Get(0), 30.0, 30.0);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalRx = 0;
    double totalTx = 0;
    double totalDelay = 0;
    uint32_t flowCount = 0;

    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort == 9) {
            totalRx += flowStats.rxPackets;
            totalTx += flowStats.txPackets;
            totalDelay += flowStats.delaySum.GetSeconds();
            flowCount++;
        }
    }

    double packetDeliveryRatio = (totalTx > 0) ? (totalRx / totalTx) : 0;
    double averageEndToEndDelay = (flowCount > 0) ? (totalDelay / flowCount) : 0;

    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
    std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}