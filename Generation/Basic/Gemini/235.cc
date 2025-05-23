#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lorawan-module.h"

using namespace ns3;
using namespace lorawan;

int main(int argc, char *argv[]) {
    // 1. Set up simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t udpPort = 9; // Echo server port
    Time packetInterval = MilliSeconds(500);
    uint32_t packetSize = 50; // bytes

    // 2. Create nodes
    NodeContainer endDevices;
    endDevices.Create(1);
    NodeContainer gateways;
    gateways.Create(1);

    Ptr<Node> sensorNode = endDevices.Get(0);
    Ptr<Node> sinkNode = gateways.Get(0);

    // 3. Install Internet Stack on nodes
    InternetStackHelper internet;
    internet.Install(sensorNode);
    internet.Install(sinkNode);

    // 4. Configure LoRaWAN network
    LoRaWanHelper lorawanHelper;

    // Set up mobility (static positions)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Sensor node position
    Ptr<ConstantPositionMobilityModel> sensorMobility = CreateObject<ConstantPositionMobilityModel>();
    sensorMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    sensorNode->AggregateObject(sensorMobility);

    // Sink node (gateway) position
    Ptr<ConstantPositionMobilityModel> sinkMobility = CreateObject<ConstantPositionMobilityModel>();
    sinkMobility->SetPosition(Vector(10.0, 0.0, 0.0)); // 10 meters away
    sinkNode->AggregateObject(sinkMobility);

    // Configure propagation loss and delay models
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetReference(1, 7.70); // LoRaWAN parameters from examples (1m, -7.7dB)
    loss->SetExponent(2.0); // Free space exponent (2.0 for free space)
    Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    // Create a LoRaWAN channel
    Ptr<LoraChannel> channel = CreateObject<LoraChannel>();
    channel->SetPropagationLossModel(loss);
    channel->SetPropagationDelayModel(delay);
    lorawanHelper.SetChannel(channel);

    // Install LoRaWAN devices
    // Gateway (sink node)
    Ptr<GatewayLoraNetDevice> gatewayNetDevice = lorawanHelper.InstallGateway(sinkNode);

    // End Device (sensor node)
    Ptr<EndDeviceLoraNetDevice> endDeviceNetDevice = lorawanHelper.InstallEndDevice(sensorNode);

    // Install Network Server on the sink node
    Ptr<NetworkServer> networkServer = lorawanHelper.InstallNetworkServer(sinkNode);
    
    // Add the gateway to the network server (often done by helper, but explicit for clarity)
    networkServer->AddGateway(gatewayNetDevice);

    // Associate end device with network server (using ABP for simplicity)
    lorawanHelper.Activate(endDeviceNetDevice, networkServer);

    // 5. Setup UDP applications
    // Get the IP address of the sink node. The NetworkServer typically creates an Ipv4 interface on the node.
    Ptr<Ipv4> sinkIpv4 = sinkNode->GetObject<Ipv4>();
    // GetAddress(1,0) refers to the first non-loopback interface (index 1), with address 0
    Ipv4Address sinkIpAddress = sinkIpv4->GetAddress(1,0).GetLocal(); 

    // UDP Server on Sink Node
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(sinkNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP Client on Sensor Node
    UdpClientHelper client(sinkIpAddress, udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(1000)); // Max packets to send
    client.SetAttribute("Interval", TimeValue(packetInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = client.Install(sensorNode);
    clientApps.Start(Seconds(1.0)); // Start sending after 1 second
    clientApps.Stop(Seconds(simulationTime - 1.0)); // Stop sending 1 second before simulation end

    // 6. Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}