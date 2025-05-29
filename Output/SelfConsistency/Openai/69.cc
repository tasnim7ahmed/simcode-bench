// dumbbell-red-fared.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellRedFaredExample");

int
main (int argc, char *argv[])
{
    // Command line parameters
    uint32_t nLeaf = 4;
    std::string queueType = "RED"; // "RED" or "FengAdaptiveRED"
    uint32_t maxPackets = 100; // Max packets in the queue
    uint32_t pktSize = 1000; // Bytes
    std::string dataRate = "10Mbps";
    std::string bottleneckRate = "2Mbps";
    std::string accessDelay = "2ms";
    std::string bottleneckDelay = "10ms";
    double minTh = 5;
    double maxTh = 15;
    double qWeight = 0.002;
    double prob = 0.02;
    double simTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue ("nLeaf", "Number of leaf nodes on each side", nLeaf);
    cmd.AddValue ("queueType", "Queue type: RED or FengAdaptiveRED", queueType);
    cmd.AddValue ("maxPackets", "Maximum packets allowed in the bottleneck queue", maxPackets);
    cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
    cmd.AddValue ("dataRate", "Access link bandwidth", dataRate);
    cmd.AddValue ("bottleneckRate", "Bottleneck link bandwidth", bottleneckRate);
    cmd.AddValue ("accessDelay", "Access link delay", accessDelay);
    cmd.AddValue ("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue ("minTh", "RED Min threshold (packets)", minTh);
    cmd.AddValue ("maxTh", "RED Max threshold (packets)", maxTh);
    cmd.AddValue ("qWeight", "RED queue weight", qWeight);
    cmd.AddValue ("prob", "RED maximum probability", prob);
    cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
    cmd.Parse (argc, argv);

    // Convert queue type to canonical name
    std::string queueDiscType;
    if (queueType == "FengAdaptiveRED" || queueType == "fengadaptive" || queueType == "FengAdaptive" ||
        queueType == "FengAdaptiveRed" || queueType == "FengAdapRed")
    {
        queueDiscType = "ns3::FqCoDelQueueDisc";
        NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe ("ns3::FengAdaptiveRedQueueDisc") != 0,
            "FengAdaptiveRedQueueDisc type not found in your ns-3 build.");

        queueDiscType = "ns3::FengAdaptiveRedQueueDisc";
    }
    else // default
    {
        NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe ("ns3::RedQueueDisc") != 0,
            "RedQueueDisc type not found in your ns-3 build.");

        queueDiscType = "ns3::RedQueueDisc";
    }

    // Set up nodes
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create (nLeaf);
    rightNodes.Create (nLeaf);
    routers.Create (2);

    // Access link parameters
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    accessLink.SetChannelAttribute ("Delay", StringValue (accessDelay));

    // Bottleneck link parameters
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneckRate));
    bottleneckLink.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

    // Install devices to left and right access links
    NetDeviceContainer leftDevices, rightDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer nd = accessLink.Install (leftNodes.Get(i), routers.Get(0));
        leftDevices.Add (nd.Get (0));
        // router side devices not added
    }
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer nd = accessLink.Install (rightNodes.Get(i), routers.Get(1));
        rightDevices.Add (nd.Get (0));
    }

    // Install bottleneck link between routers
    NetDeviceContainer routerDevices = bottleneckLink.Install (routers);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (leftNodes);
    internet.Install (rightNodes);
    internet.Install (routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInterfaces, rightInterfaces;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign (
            NetDeviceContainer (leftDevices.Get(i), routers.Get(0)->GetDevice(i))
        );
        leftInterfaces.push_back (iface);
    }
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign (
            NetDeviceContainer (rightDevices.Get(i), routers.Get(1)->GetDevice(i))
        );
        rightInterfaces.push_back (iface);
    }
    // Bottleneck link
    address.SetBase ("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfaces = address.Assign (routerDevices);

    // Set up Traffic Control (QueueDisc) on bottleneck
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc (queueDiscType);
    tch.SetQueueLimits ("ns3::QueueLimits", "MaxPackets", UintegerValue (maxPackets));
    if (queueDiscType == "ns3::RedQueueDisc" || queueDiscType == "ns3::FengAdaptiveRedQueueDisc")
    {
        tch.SetRootQueueDisc (queueDiscType,
            "QueueLimit", UintegerValue (maxPackets),
            "MinTh", DoubleValue (minTh),
            "MaxTh", DoubleValue (maxTh),
            "QW", DoubleValue (qWeight),
            "LInterm", DoubleValue (prob)
        );
    }
    // Attach queue disc only to the *first* device of the bottleneck link (router 0's device)
    QueueDiscContainer qdiscs = tch.Install (routerDevices.Get (0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Install TCP OnOffApplications on right-side leaf nodes, sink on left-side leaf
    uint16_t port = 50000;
    ApplicationContainer apps, sinks;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        // On left router interface for flow i
        Ipv4Address sinkAddr = leftInterfaces[i].GetAddress (0);

        // Sink
        Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port + i));
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install (leftNodes.Get(i));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (simTime + 1));
        sinks.Add (sinkApp);

        // OnOff app on right node
        OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (sinkAddr, port + i));
        onoff.SetAttribute ("DataRate", StringValue (bottleneckRate));
        onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));

        // On/Off times: UniformRandomVariable for both
        Ptr<UniformRandomVariable> onTime = CreateObject<UniformRandomVariable> ();
        onTime->SetAttribute ("Min", DoubleValue (0.5));
        onTime->SetAttribute ("Max", DoubleValue (1.0));
        Ptr<UniformRandomVariable> offTime = CreateObject<UniformRandomVariable> ();
        offTime->SetAttribute ("Min", DoubleValue (0.1));
        offTime->SetAttribute ("Max", DoubleValue (0.5));
        onoff.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.5|Max=1.0]"));
        onoff.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));

        ApplicationContainer app = onoff.Install (rightNodes.Get(i));
        app.Start (Seconds (1.0));
        app.Stop (Seconds (simTime));
        apps.Add (app);
    }

    // Enable flow monitor (optional, not required for drop stats)
    // FlowMonitorHelper flowmon;
    // Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop (Seconds (simTime + 2));
    Simulator::Run ();

    // Gather QueueDisc statistics
    Ptr<QueueDisc> q = qdiscs.Get (0);
    std::cout << "*** Queue Disc Statistics at the end of simulation ***\n";
    std::cout << "Type: " << q->GetInstanceTypeId ().GetName () << std::endl;
    std::cout << "  Packets (current): " << q->GetCurrentSize ().GetValue () << std::endl;
    std::cout << "  Total Packets Received: " << q->GetTotalReceivedPackets () << std::endl;
    std::cout << "  Total Packets Dropped: " << q->GetTotalDroppedPackets () << std::endl;
    std::cout << "  Total Packets Marked: " << q->GetTotalMarkedPackets () << std::endl;

    // Require that some packets were dropped (to validate RED operation)
    uint32_t drops = q->GetTotalDroppedPackets ();
    if (drops == 0) {
        std::cout << "Warning: No packets were dropped by the queue disc. Check RED/FARED parameters.\n";
    } else {
        std::cout << "OK: " << drops << " packets dropped (expected for RED/FARED test).\n";
    }

    Simulator::Destroy ();
    return 0;
}