#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t numSensors = 5;
    double simulationTime = 60.0;
    bool verbose = true;
    std::string animFile = "sensor_network.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numSensors", "Number of sensor nodes.", numSensors);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer sensors;
    sensors.Create(numSensors);
    NodeContainer sinkNode;
    sinkNode.Create(1);

    YansLrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannelParameter("Delay", TimeValue(Seconds(0.0001)));
    lrWpanHelper.SetPdDataRequestCallback(MakeCallback(&LrWpanPhyDataIndication));
    lrWpanHelper.SetPlmeCcaConfirmCallback(MakeCallback(&LrWpanMacStateChange));

    NetDeviceContainer sensorDevices;
    NetDeviceContainer sinkDevice;

    for (uint32_t i = 0; i < numSensors; ++i) {
        sensorDevices.Add(lrWpanHelper.Install(sensors.Get(i)));
    }
    sinkDevice.Add(lrWpanHelper.Install(sinkNode.Get(0)));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensors);
    mobility.Install(sinkNode);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer sinkInterface = ipv4.Assign(sinkDevice);
    ipv4.Assign(sensorDevices);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(sinkNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(sinkInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSensors; ++i) {
        clientApps.Add(client.Install(sensors.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    lrWpanHelper.EnableLogComponents();

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(sinkNode.Get(0), 50.0, 50.0);
    for (uint32_t i = 0; i < numSensors; ++i) {
        anim.UpdateNodeDescription(sensors.Get(i), "Sensor");
        anim.UpdateNodeColor(sensors.Get(i), 255, 0, 0);
    }
    anim.UpdateNodeDescription(sinkNode.Get(0), "Sink");
    anim.UpdateNodeColor(sinkNode.Get(0), 0, 0, 255);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier())->FindFlow(it->first);
        std::cout << "Flow ID: " << it->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress
                  << " Packet Delivery Ratio: " << (double)it->second.rxPackets / (double)it->second.txPackets
                  << " Avg Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets
                  << " Throughput: " << (it->second.rxBytes * 8.0) / simulationTime / 1000.0 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}