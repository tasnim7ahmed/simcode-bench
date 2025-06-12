#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Nodes: eNodeB and UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Create LTE Helper
    LteHelper lteHelper;
    lteHelper.SetEnbAntennaModelType("ns3::IsotropicAntennaModel");

    // Create EPC Helper
    Ptr<LteEpcLmmMme> epcHelper = CreateObject<LteEpcLmmMme>();
    lteHelper.SetEpcHelper(epcHelper);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(enbNodes);


    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP Addresses
    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer enbIpIface;
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = internet.AssignIpv4Addresses(ueDevs);
    enbIpIface = internet.AssignIpv4Addresses(enbDevs);


    // Attach UE to eNodeB
    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    // Set active protocol
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
    EpsBearer bearer(q);
    lteHelper.ActivateDataRadioBearer(ueNodes, bearer);

    // Install and start applications on UE (server) and eNodeB (client)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}