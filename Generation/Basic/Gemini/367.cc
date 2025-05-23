#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set default simulation time
    double simTime = 10.0;

    // Create LTE helper and EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create mobility helper
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Create eNodeB node
    NodeContainer enbNodes;
    enbNodes.Create(1);
    mobility.Install(enbNodes.Get(0));
    enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // Create UE node
    NodeContainer ueNodes;
    ueNodes.Create(1);
    mobility.Install(ueNodes.Get(0));
    ueNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));

    // Install the IP stack on the eNodeB and UE nodes
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Install LTE devices on the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Get the eNodeB's S1-U interface IP address
    // The EpcHelper sets up a PointToPointNetDevice on one of the eNB's interfaces
    Ipv4Address enbS1uAddr;
    Ptr<Ipv4> ipv4Enb = enbNodes.Get(0)->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4Enb->GetNInterfaces(); ++i)
    {
        Ipv4InterfaceAddress ifaceAddr = ipv4Enb->GetAddress(i, 0);
        if (ifaceAddr.GetLocal() != Ipv4Address::GetLoopback())
        {
            enbS1uAddr = ifaceAddr.GetLocal();
            break;
        }
    }

    // Set up PacketSink application on the eNodeB (server)
    uint16_t port = 9; // Discard port for testing
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Set up BulkSendApplication on the UE (client)
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(enbS1uAddr, port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely until simulation ends
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(0.1)); // Start slightly after the server to ensure server is ready
    clientApps.Stop(Seconds(simTime));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}