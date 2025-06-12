#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for EpcHelper and TcpClientServerApplication
    LogComponentEnable("EpcHelper", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClientServerApplication", LOG_LEVEL_INFO);

    // Create 2 UEs and 1 eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2);

    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNodes);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Get first UE as server, second as client
    Ptr<Node> ue0 = ueNodes.Get(0);
    Ptr<Node> ue1 = ueNodes.Get(1);

    Ipv4Address serverAddr = ueIpIfaces.GetAddress(0, 0);

    // Install TCP server on UE0
    uint16_t port = 5000;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(ue0);
    sinkApp.Start(Seconds(0.1));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP client on UE1
    InetSocketAddress remoteAddress(serverAddr, port);
    remoteAddress.SetTos(0xb8); // DSCP AF41

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(1000000)); // Send ~1MB of data
    ApplicationContainer clientApp = bulkSend.Install(ue1);
    clientApp.Start(Seconds(0.2));
    clientApp.Stop(Seconds(10.0));

    // Setup animation output
    AnimationInterface anim("lte-tcp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Assign node colors in animation
    anim.UpdateNodeDescription(ueNodes.Get(0), "UE-0");
    anim.UpdateNodeDescription(ueNodes.Get(1), "UE-1");
    anim.UpdateNodeDescription(enbNodes.Get(0), "eNodeB");
    anim.UpdateNodeColor(ueNodes.Get(0), 255, 0, 0);   // Red
    anim.UpdateNodeColor(ueNodes.Get(1), 0, 0, 255);   // Blue
    anim.UpdateNodeColor(enbNodes.Get(0), 0, 255, 0);  // Green

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}