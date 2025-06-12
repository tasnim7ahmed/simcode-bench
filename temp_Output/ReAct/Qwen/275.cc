#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation resolution
    Time::SetResolution(Time::NS);

    // Create eNB and UE nodes
    NodeContainer enbNode;
    enbNode.Create(1);
    NodeContainer ueNode;
    ueNode.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNode);

    // Attach UE to eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install the IP stack on all nodes
    InternetStackHelper internet;
    internet.Install(enbNode);
    internet.Install(ueNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UE
    Ipv4Address enbIpAddress = ueIpIfaces.GetAddress(0, 1); // Get eNB side address
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> ueIpv4 = ueNode.Get(0)->GetObject<Ipv4>();
    uint32_t ueDefaultRouteIndex = ueIpv4->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), enbIpAddress, 1);
    NS_ASSERT_MSG(ueDefaultRouteIndex == 0, "Failed to add default route for UE");

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Distance", DoubleValue(100.0));
    mobility.Install(ueNode);

    // Enable logging
    lteHelper->EnableTraces();

    // Install applications
    uint16_t dlPort = 1234;

    // Create UDP server (on eNB)
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer dlServerApp = dlServer.Install(enbNode.Get(0));
    dlServerApp.Start(Seconds(0.0));
    dlServerApp.Stop(Seconds(10.0));

    // Create UDP client (on UE)
    UdpClientHelper dlClient(enbIpAddress, dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(5));
    dlClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    dlClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer dlClientApp = dlClient.Install(ueNode.Get(0));
    dlClientApp.Start(Seconds(1.0));
    dlClientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}