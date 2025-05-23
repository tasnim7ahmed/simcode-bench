#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/http-client-server-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    double simTime = 10.0; // seconds

    // 1. Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // 2. Create nodes (eNodeB and UE)
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // 3. Configure Mobility
    // eNodeB: Static (ConstantPositionMobilityModel)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);
    enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // UE: Random Walk Mobility Model within a 50x50 area
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", StringValue("0|50|0|50"),
                                "Distance", DoubleValue(5.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    ueMobility.Install(ueNodes);

    // 4. Install LTE devices on nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // 5. Install the IP stack on UEs and the PGW node
    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(ueNodes);
    internetStackHelper.Install(epcHelper->GetPgwNode());

    // 6. Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(ueLteDevs);

    // 7. Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // 8. Setup HTTP Server on PGW
    // The prompt asks for a "UDP server" on eNodeB but then specifies "HTTP communication"
    // and "HTTP client" on port 80. HTTP uses TCP. Placing the server on the PGW
    // is standard for UE-to-server communication in LTE.
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    Ipv4Address serverAddress = pgw->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(); // Get IP of PGW's public interface

    HttpClientServerHelper serverHelper(80); // HTTP Server on port 80
    ApplicationContainer serverApps = serverHelper.Install(pgw);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // 9. Setup HTTP Client on UE
    HttpClientHelper clientHelper(serverAddress, 80); // Connect to server on PGW, port 80
    clientHelper.SetAttribute("MaxPackets", UintegerValue(5)); // Send 5 packets
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second intervals
    
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client after server is initialized
    clientApps.Stop(Seconds(simTime));

    // 10. Start simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}