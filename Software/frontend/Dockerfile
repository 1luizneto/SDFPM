# Usa imagem do Node
FROM node:22-alpine

WORKDIR /app

# Copia os arquivos de dependência
COPY package.json package-lock.json ./

# Instala as dependências
RUN npm install

# Copia o código fonte
COPY . .

# Expõe a porta do Vite
EXPOSE 5173

# Roda o servidor de desenvolvimento liberando acesso externo (--host)
CMD ["npm", "run", "dev", "--", "--host"]