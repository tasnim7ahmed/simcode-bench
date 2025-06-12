#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/6lowpan-net-device.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoT6LoWPANSimulation");

int main(int argc, char *argv[]) {
    uint32_t numIotDevices = 5;
    double simulationTime = 60.0;
    double packetInterval = 2.0;
    uint32_t packetSize = 128;

    CommandLine cmd;
    cmd.AddValue("numIotDevices", "Number of IoT devices", numIotDevices);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("packetInterval", "Packet interval in seconds", packetInterval);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.Parse(argc, argv);

    NodeContainer iotNodes;
    iotNodes.Create(numIotDevices);

    NodeContainer serverNode;
    serverNode.Create(1);

    YansLrWpanPhyHelper lrWpanPhy = YansLrWpanPhyHelper::Default();
    LrWpanMacHelper lrWpanMac = LrWpanMacHelper::Default();
    lrWpanMac.SetType("ns3::Dot15d4Mac",
                      "m_maxCsmaBackoffs", UintegerValue(4),
                      "m_minBe", UintegerValue(3));

    NetDeviceContainer serverDevice;
    NetDeviceContainer iotDevices;

    lrWpanPhy.SetPcapDataLinkType(SpectrumChannelHelper::DLT_IEEE802_15_4);
    serverDevice = lrWpanPhy.Install(serverNode);
    lrWpanMac.Install(serverNode, serverDevice);

    iotDevices = lrWpanPhy.Install(iotNodes);
    lrWpanMac.Install(iotNodes, iotDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(iotNodes);
    mobility.Install(serverNode);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer serverInterface;
    Ipv6InterfaceContainer iotInterfaces;

    serverInterface = ipv6.Assign(serverDevice);
    serverInterface.SetForwarding(0, true);
    serverInterface.SetDefaultRouteInAllNodes(0);

    iotInterfaces = ipv6.Assign(iotDevices);
    iotInterfaces.SetForwarding(0, false);

    for (uint32_t i = 0; i < numIotDevices; ++i) {
        Ipv6StaticRoutingHelper routingHelper;
        Ptr<Ipv6> serverIpv6 = serverNode.Get(0)->GetObject<Ipv6>();
        Ipv6InterfaceAddress serverAddr = serverIpv6->GetInterface(0)->GetAddress(0, 0);
        Ipv6Address serverIp = serverAddr.GetAddress();

        Ptr<Ipv6StaticRouting> routing = routingHelper.GetStaticRouting(iotNodes.Get(i)->GetObject<Ipv6>());
        routing->AddHostRouteTo(serverIp, 1);
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(serverNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    ApplicationContainer clientApps;
    UdpEchoClientHelper echoClient(Ipv6Address::GetZero(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    for (uint32_t i = 0; i < numIotDevices; ++i) {
        ApplicationContainer app = echoClient.Install(iotNodes.Get(i));
        app.Start(Seconds(2.0));
        app.Stop(Seconds(simulationTime));
        clientApps.Add(app);
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("iot-6lowpan.tr");
    lrWpanPhy.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}