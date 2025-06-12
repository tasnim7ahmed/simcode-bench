#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: one for eNB and one for UE
    NodeContainer enbNode;
    enbNode.Create(1);

    NodeContainer ueNode;
    ueNode.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNode);

    // Install the IP stack on the UE
    InternetStackHelper internet;
    internet.Install(ueNode);

    // Assign IP addresses to UE
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to the eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Create UDP server (on UE)
    UdpEchoServerHelper echoServer(12345);
    ApplicationContainer serverApps = echoServer.Install(ueNode.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(10.0));

    // Create UDP client (on eNB)
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), 12345);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(enbNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);
    mobility.Install(ueNode);

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}