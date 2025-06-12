#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for applications
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Set up simulation parameters
    uint32_t numSensorNodes = 5;
    uint32_t numBaseStations = 1;
    uint32_t totalNodes = numSensorNodes + numBaseStations;
    double simDuration = 10.0;

    NodeContainer sensorNodes;
    sensorNodes.Create(numSensorNodes);

    NodeContainer baseStation;
    baseStation.Create(numBaseStations);

    NodeContainer allNodes;
    allNodes.Add(sensorNodes);
    allNodes.Add(baseStation);

    // Install CSMA devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(allNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

    // Set up UDP server (base station) on port 9
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(baseStation);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simDuration));

    // Set up UDP clients (sensor nodes)
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        UdpClientHelper udpClient(interfaces.GetAddress(totalNodes - 1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(sensorNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.5)); // staggered start times
        clientApp.Stop(Seconds(simDuration));
    }

    // Setup mobility to place nodes in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Configure animation output using NetAnim
    AnimationInterface anim("wsn-animation.xml");
    anim.SetMobilityPollInterval(Seconds(1));
    for (uint32_t i = 0; i < totalNodes; ++i) {
        anim.UpdateNodeDescription(allNodes.Get(i), "Sensor");
        anim.UpdateNodeColor(allNodes.Get(i), 255, 0, 0); // red
    }
    anim.UpdateNodeDescription(baseStation.Get(0), "BaseStation");
    anim.UpdateNodeColor(baseStation.Get(0), 0, 0, 255); // blue

    // Enable pcap tracing for throughput analysis
    csma.EnablePcapAll("wsn-packet-trace");

    // Setup flow monitor to collect metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Print performance metrics
    std::cout << "\nPerformance Metrics:\n";
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationPort == 9) { // Server port
            std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Packets Sent:   " << iter->second.txPackets << "\n";
            std::cout << "  Packets Received: " << iter->second.rxPackets << "\n";
            std::cout << "  Packet Delivery Ratio: " << (double)iter->second.rxPackets / (double)iter->second.txPackets * 100.0 << "%\n";
            std::cout << "  Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << "s\n";
        }
    }

    Simulator::Destroy();
    return 0;
}