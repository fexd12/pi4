#include "Servo.h"
//------------------------------------------------------------------------------------
//------------------------------Classe LDR--------------------------------------------
//Descricao: Cada objeto dessa classe representa um LDR. A classe tem funcao de
// salvar a pino de leitura do LDR e realizar a leitura
//Autor: Laudelino Adao Junior, Vinicius Silva Mazzola, Juan Pietro e Sergio Augusto.
//------------------------------------------------------------------------------------
class LDR
{
 private:
 int sensorPin;
 double rvalue;
 double resistencia;
 double Vldr;

 public:
 void init(int sp)
 {
 sensorPin = sp;
 }

 //Faz uma media de 3 leituras em um intervalo de 30ms e retorna a resistencia
 double readLDR()
 {
 Vldr = 0;

 for(int i =0; i<3; i++){
 delay(10);
 rvalue = analogRead(sensorPin);
 Vldr += rvalue*5/1024;
 }

 Vldr = Vldr/3;
 resistencia = (Vldr*10000)/(5-Vldr);

 return resistencia;
 }
};
//------------------------------------------------------------------------------------
//------------------------------Classe Sensing----------------------------------------
//Descricao: é composta pelos LDR, ela tem funcao de pedir a leitura de cada LDR e
// analisar a necessidade de movimento.
//Autor: Laudelino Adao Junior, Vinicius Silva Mazzola, Juan Pietro e Sergio Augusto.
//------------------------------------------------------------------------------------
class Sensing
{
 private:
 LDR top, bottom, left, right;
 int pos;
 double err;
 String girar;

 public:
 void init()
 {
 err = 0;

 top.init(36);//Top
 bottom.init(39);//Botton
 left.init(34);//Left
 right.init(35);//Right

 pos = -1;//Botton = 1; Top = 2; Left = 3; Right = 4;
 }
 int compare(){
 double media = 0, rTop = top.readLDR(), rBottom = bottom.readLDR(), rLeft = left.readLDR(), rRight = right.readLDR();
 pos = 0;
 girar = "";

 err = 0.20; //O resistor tem uma margem de erro de 5%, ja o LDR eh responsavelpelos outros 13%
 //O erro é multiplicado pela media entre o sendo verificado e o oposto dele.

 if(rTop < rBottom && rTop < rLeft && rTop < rRight){//Verifica se o menor eh o do topo
 
 pos = 2; 
 girar = "Top";
 media = (rTop/2 + rBottom/2); // Calcula a media com o oposto
 err*= media;
 }
 else if(rBottom < rLeft && rBottom < rRight)
 {
 pos = 1;
 girar = "Bottom";
 media = (rTop/2 + rBottom/2); // Calcula a media com o oposto
 err*= media;
 }
 else if(rLeft<rRight)
 {
 pos = 3;
 girar = "Left";
 media = (rLeft/2 + rRight/2);// Calcula a media com o oposto
 err*= media;
 }
 else
 {
 pos = 4;
 girar = "Right";
 media = (rLeft/2 + rRight/2); // Calcula a media com o oposto
 err*= media;
 }
 /* Serial.println("");
 Serial.println(pos);
 Serial.print("ERRO: ");
 Serial.println(err);
 Serial.print("Girar para: ");
 Serial.println(girar);
 Serial.println("TODOS:");
 Serial.print("Bottom = ");
 Serial.println(rBottom);
 Serial.print("Top = ");
 Serial.println(rTop);
 Serial.print("Right = ");
 Serial.println(rRight);
 Serial.print("Left = ");
 Serial.println(rLeft);
 Serial.println("");*/
 //verifica se o modulo da diferenca com o oposto eh maior que o erro, se for, retornapara onde deve girar, senao retorna 0
 switch(pos)
 {
 case 1:{
 if(abs(rTop-rBottom) > err)
 return pos;
 else
 return 0;
 }
 case 2: {
 if(abs(rTop-rBottom) > err)
 return pos;
 else
 return 0;
 }
 case 3:{
 if(abs(rLeft-rRight) > err)
 return pos;
 else
 return 0;
 }
 case 4:{
 if(abs(rLeft-rRight) > err)
 return pos;
 else
 return 0;
 }
 default: return 0;
 }
 }
};
//------------------------------------------------------------------------------------
//------------------------------Classe Actuator---------------------------------------
//Descricao: Conecta e desconecta os servos motores em um pino, define a posicao
// inicial e gira conforme a necessidade
//Autor: Laudelino Adao Junior, Vinicius Silva Mazzola, Juan Pietro e Sergio Augusto.
//------------------------------------------------------------------------------------
class Actuator
{
 private:
 Servo xServo, yServo;
 int xTheta, yTheta, xRot, yRot, xPin, yPin;

 public:
 void init()
 {
 yPin = 32;
 xPin = 33;
 xTheta = 90;
 yTheta = 90;
 xRot = 5;//Valor de rotacao no eixo X
 yRot = 5;//Valor de rotacao no eixo X

 //Inicializa ambos os servos em 90 graus
 yServo.attach(yPin);
 yServo.write(yTheta);
 delay(120);
 yServo.detach();

 xServo.attach(xPin);

 xServo.write(xTheta);
 delay(120);
 xServo.detach();

}

 void rotateTop(){
 yTheta = yServo.read();
    if((yTheta + yRot) > 180){
 //nao pode mais girar para baixo
        Serial.println("No rotation possible");
        delay(5);
    }
 else{
 yTheta += yRot;//Gira para direcao indicada
 yServo.attach(yPin);
 yServo.write(yTheta);
 delay(120);
 yServo.detach();

}

}
 void rotateBottom()

{
 yTheta = yServo.read();
 if((yTheta
- yRot) < 0)

{
 //nao pode mais girar para baixo
 Serial.println("No rotation possible");
 delay(5);
 }
 else
{
 yTheta-= yRot;//Gira para direcao indicada
 yServo.attach(yPin);
 yServo.write(yTheta);
 delay(120);
 yServo.detach();

}

}
 void rotateLeft()

{
 xTheta = xServo.read();
 if((xTheta + xRot) > 180)

{
 //nao pode mais girar para baixo
 Serial.println("No rotation possible");
 delay(5);
 }
 else

{
 xTheta += xRot;//Gira para direcao indicada
 xServo.attach(xPin);
 xServo.write(xTheta);

 delay(120);
 xServo.detach();
 }
 }
 void rotateRight()
 {
 xTheta = xServo.read();
 if((xTheta - xRot) < 0)
 {
 //nao pode mais girar para baixo
 Serial.println("No rotation possible");
 delay(5);
 }
 else
 {
 xTheta -= xRot;//Gira para direcao indicada
 xServo.attach(xPin);
 xServo.write(xTheta-xRot);
 delay(120);
 xServo.detach();
 }
 }
};
//------------------------------------------------------------------------------------
//------------------------------Classe Control----------------------------------------
//Descricao: Une as classes sensing e control, pede a posicao para qual girar para a
// sensing e manda girar o necessario na control
//Autor: Laudelino Adao Junior, Vinicius Silva Mazzola, Juan Pietro e Sergio Augusto.
//------------------------------------------------------------------------------------
class Control
{
 private:
 Sensing s;
 Actuator a;
 int pos;

 public:
 void init()
 {
 s.init();
 a.init();
 pos = 0;
 }

//Busca se e para onde deve mover, e chama o metodo correspondente para ocorreresse movimento.
 void move()
 {
 pos = s.compare();
 switch(pos)
 {
 case 0: Serial.println("No move neded"); break;
 case 1: a.rotateBottom(); Serial.println("Moving down"); break;
 case 2: a.rotateTop(); Serial.println("Moving up"); break;

 case 3: a.rotateLeft(); Serial.println("Moving left"); break;
 case 4: a.rotateRight(); Serial.println("Moving right"); break;
 default: return;
 }
 }
};
/*Control c;

void setup()
{
 Serial.begin(9600);
 c.init();
}
void loop()
{
 c.move();
 delay(100);
}
*/