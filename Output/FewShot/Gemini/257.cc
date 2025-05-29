#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("LteUdp", LOG_LEVEL_INFO);

    // Create LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create EPC Helper
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host (for SGi interface)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the Internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHostContainer, epcHelper->GetSgw());
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    // Set the default gateway for the UE
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(epcHelper->GetUeNetwork(), Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Create eNodeB and UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Place eNodeB and UEs in a grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(10.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);


    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

    // Activate a data radio bearer between UE and eNodeB
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueNodes.Get(0), bearer);
    lteHelper->ActivateDataRadioBearer(ueNodes.Get(1), bearer);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set the default gateway for the UE
    Ptr<Ipv4StaticRouting> ue0StaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ue0StaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    Ptr<Ipv4StaticRouting> ue1StaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(1)->GetObject<Ipv4>());
    ue1StaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Create UDP application to send from UE0 to UE1
    uint16_t port = 9;
    UdpClientHelper client(ueIpIface.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(2));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));


    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}